// Resampler.cpp
//
// IR emission for one ResamplerHermite step. Mirrors ResamplerHermiteStage
// from libs/compiled exactly: 4-sample FIFO ring, Hermite cubic interpolation,
// fixed-interval emission. May invoke the callback 0, 1, or many times.
//
// State layout (state_offset + 11 doubles):
//   [0..3]  ring_v[4]    — value ring
//   [4..7]  ring_t[4]    — timestamp ring (bit-cast i64 <-> double)
//   [8]     next_emit    — next emission timestamp (bit-cast i64 <-> double)
//   [9]     initialized  — 0.0=false / 1.0=true
//   [10]    count        — sample count (double)
//
// The interval (dt) is baked in as an i64 constant.

#include "rtbot/compiled/jit/emit/Resampler.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

void emit_resampler_hermite(IrEmissionContext& ec,
                             std::size_t state_offset,
                             std::int64_t interval,
                             llvm::Value* t, llvm::Value* v,
                             ResamplerEmitCallback emit_cb) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  auto ci = [&](int64_t val) -> llvm::Value* {
    return llvm::ConstantInt::get(i64, val);
  };
  auto cf = [&](double val) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, val);
  };

  // State slot offsets relative to state_offset.
  const std::size_t RING_V0   = state_offset + 0;
  const std::size_t RING_V1   = state_offset + 1;
  const std::size_t RING_V2   = state_offset + 2;
  const std::size_t RING_V3   = state_offset + 3;
  const std::size_t RING_T0   = state_offset + 4;
  const std::size_t RING_T1   = state_offset + 5;
  const std::size_t RING_T2   = state_offset + 6;
  const std::size_t RING_T3   = state_offset + 7;
  const std::size_t NEXT_EMIT = state_offset + 8;
  const std::size_t INIT      = state_offset + 9;
  const std::size_t COUNT     = state_offset + 10;

  // GEP helpers for static slots.
  auto slot = [&](std::size_t s) -> llvm::Value* { return ec.state_gep(s); };

  // Load/store helpers (double slots).
  auto ld = [&](std::size_t s, const llvm::Twine& name = "") -> llvm::Value* {
    return b.CreateLoad(f64, slot(s), name);
  };
  auto st = [&](std::size_t s, llvm::Value* val) {
    b.CreateStore(val, slot(s));
  };

  llvm::Value* zero_d = cf(0.0);
  llvm::Value* one_d  = cf(1.0);
  llvm::Value* four_d = cf(4.0);

  llvm::Function* fn = b.GetInsertBlock()->getParent();

  llvm::BasicBlock* bb_lt4       = llvm::BasicBlock::Create(ctx, "rs_lt4",      fn);
  llvm::BasicBlock* bb_ge4       = llvm::BasicBlock::Create(ctx, "rs_ge4",      fn);
  llvm::BasicBlock* bb_push_done = llvm::BasicBlock::Create(ctx, "rs_push_done",fn);
  llvm::BasicBlock* bb_init_chk  = llvm::BasicBlock::Create(ctx, "rs_init_chk", fn);
  llvm::BasicBlock* bb_do_init   = llvm::BasicBlock::Create(ctx, "rs_do_init",  fn);
  llvm::BasicBlock* bb_loop_hdr  = llvm::BasicBlock::Create(ctx, "rs_loop_hdr", fn);
  llvm::BasicBlock* bb_loop_body = llvm::BasicBlock::Create(ctx, "rs_loop_body",fn);
  llvm::BasicBlock* bb_loop_next = llvm::BasicBlock::Create(ctx, "rs_loop_next",fn);
  llvm::BasicBlock* bb_loop_exit = llvm::BasicBlock::Create(ctx, "rs_loop_exit",fn);

  // ---------------------------------------------------------------------------
  // Push (t, v) into the 4-sample FIFO ring.
  // ---------------------------------------------------------------------------
  llvm::Value* rs_cnt = ld(COUNT, "rs_cnt");
  b.CreateCondBr(b.CreateFCmpOLT(rs_cnt, four_d, "rs_lt4c"), bb_lt4, bb_ge4);

  // --- rs_lt4: fill from the beginning (count < 4) -------------------------
  b.SetInsertPoint(bb_lt4);
  {
    llvm::Value* idx = b.CreateFPToUI(rs_cnt, i64, "rs_idx_lt4");
    // ring_v[idx] = v
    llvm::Value* rv_base = ec.state_gep(RING_V0);
    b.CreateStore(v, b.CreateGEP(f64, rv_base, idx, "rs_rv_ptr"));
    // ring_t[idx] = bit_cast(t, double)
    llvm::Value* rt_base = ec.state_gep(RING_T0);
    b.CreateStore(b.CreateBitCast(t, f64, "rs_t_bc"),
                  b.CreateGEP(f64, rt_base, idx, "rs_rt_ptr"));
    st(COUNT, b.CreateFAdd(rs_cnt, one_d, "rs_cnt1"));
  }
  b.CreateBr(bb_push_done);

  // --- rs_ge4: shift ring left (count >= 4) --------------------------------
  b.SetInsertPoint(bb_ge4);
  {
    // Shift values left.
    st(RING_V0, ld(RING_V1, "v1")); st(RING_V1, ld(RING_V2, "v2"));
    st(RING_V2, ld(RING_V3, "v3")); st(RING_V3, v);
    // Shift timestamps left.
    st(RING_T0, ld(RING_T1, "t1")); st(RING_T1, ld(RING_T2, "t2"));
    st(RING_T2, ld(RING_T3, "t3"));
    st(RING_T3, b.CreateBitCast(t, f64, "rs_t_bc2"));
    // count stays >= 4, no increment needed.
  }
  b.CreateBr(bb_push_done);

  // --- push_done: check initialized ----------------------------------------
  b.SetInsertPoint(bb_push_done);
  b.CreateCondBr(
      b.CreateFCmpOEQ(ld(INIT, "rs_init"), zero_d, "rs_not_init"),
      bb_init_chk, bb_loop_hdr);

  // --- init_chk: only proceed if ring now has >= 4 samples -----------------
  b.SetInsertPoint(bb_init_chk);
  // Count may have been updated by rs_lt4; reload.
  llvm::Value* cnt2 = ld(COUNT, "rs_cnt2");
  b.CreateCondBr(
      b.CreateFCmpOGE(cnt2, four_d, "rs_ge4c"),
      bb_do_init, bb_loop_exit);

  // --- do_init: set next_emit = ring_t[1], mark initialized ----------------
  b.SetInsertPoint(bb_do_init);
  st(NEXT_EMIT, ld(RING_T1, "rs_rt1_init"));
  st(INIT, one_d);
  b.CreateBr(bb_loop_hdr);

  // --- loop_hdr: while (ring_t[1] <= next_emit <= ring_t[2]) --------------
  // Timestamps are stored as bit-cast doubles; compare as i64 (signed).
  b.SetInsertPoint(bb_loop_hdr);
  {
    llvm::Value* rt1 = b.CreateBitCast(ld(RING_T1, "lrt1"), i64, "rt1i");
    llvm::Value* rt2 = b.CreateBitCast(ld(RING_T2, "lrt2"), i64, "rt2i");
    llvm::Value* ne  = b.CreateBitCast(ld(NEXT_EMIT, "lne"), i64, "nei");
    llvm::Value* in_range = b.CreateAnd(
        b.CreateICmpSLE(rt1, ne, "rs_lo"),
        b.CreateICmpSLE(ne, rt2, "rs_hi"), "rs_in");
    b.CreateCondBr(in_range, bb_loop_body, bb_loop_exit);
  }

  // --- loop_body: Hermite interpolation + callback -------------------------
  b.SetInsertPoint(bb_loop_body);
  {
    // Reload timestamps for interpolation.
    llvm::Value* rt1i = b.CreateBitCast(ld(RING_T1, "brt1"), i64, "brt1i");
    llvm::Value* rt2i = b.CreateBitCast(ld(RING_T2, "brt2"), i64, "brt2i");
    llvm::Value* nei  = b.CreateBitCast(ld(NEXT_EMIT, "bne"), i64, "bnei");

    // mu = (next_emit - ring_t[1]) / (ring_t[2] - ring_t[1])
    llvm::Value* mu = b.CreateFDiv(
        b.CreateSIToFP(b.CreateSub(nei, rt1i, "mu_n"), f64, "mu_nd"),
        b.CreateSIToFP(b.CreateSub(rt2i, rt1i, "mu_d"), f64, "mu_dd"),
        "mu");

    llvm::Value* y0 = ld(RING_V0, "rs_y0");
    llvm::Value* y1 = ld(RING_V1, "rs_y1");
    llvm::Value* y2 = ld(RING_V2, "rs_y2");
    llvm::Value* y3 = ld(RING_V3, "rs_y3");

    llvm::Value* rv = ec.emit_hermite_interp(y0, y1, y2, y3, mu);

    // Invoke the downstream callback with (next_emit as i64, interpolated_value).
    emit_cb(nei, rv);
  }
  b.CreateBr(bb_loop_next);

  // --- loop_next: advance next_emit by dt ----------------------------------
  b.SetInsertPoint(bb_loop_next);
  {
    llvm::Value* ne_i = b.CreateBitCast(ld(NEXT_EMIT, "ne_nx"), i64, "ne_i_nx");
    st(NEXT_EMIT, b.CreateBitCast(
        b.CreateAdd(ne_i, ci(interval), "ne_adv"), f64, "ne_adv_d"));
  }
  b.CreateBr(bb_loop_hdr);

  // --- loop_exit: IRBuilder positioned here for caller ---------------------
  b.SetInsertPoint(bb_loop_exit);
}

}  // namespace rtbot::jit::emit
