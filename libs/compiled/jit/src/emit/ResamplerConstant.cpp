// ResamplerConstant.cpp
//
// IR emission for one ResamplerConstant step. Mirrors the FE
// ResamplerConstant<NumberData>::process_data exactly: zero-order-hold
// resampling at a fixed grid (dt, optional anchor t0, optional snap_first).
//
// State layout (state_offset + 3 doubles):
//   [0] last_value   — most recent input value
//   [1] next_emit    — next emission timestamp (bit-cast i64 <-> double)
//   [2] initialized  — 0.0=false / 1.0=true
//
// dt, t0, snap_first are baked in as compile-time constants.

#include "rtbot/compiled/jit/emit/ResamplerConstant.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

void emit_resampler_constant(IrEmissionContext& ec,
                              std::size_t state_offset,
                              std::int64_t interval, bool t0_set,
                              std::int64_t t0, bool snap_first,
                              llvm::Value* t, llvm::Value* v,
                              ResamplerConstantEmitCallback emit_cb) {
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

  const std::size_t LAST_V    = state_offset + 0;
  const std::size_t NEXT_EMIT = state_offset + 1;
  const std::size_t INIT      = state_offset + 2;

  auto slot = [&](std::size_t s) { return ec.state_gep(s); };
  auto ld_d = [&](std::size_t s, const llvm::Twine& nm = "") {
    return b.CreateLoad(f64, slot(s), nm);
  };
  auto st_d = [&](std::size_t s, llvm::Value* val) {
    b.CreateStore(val, slot(s));
  };

  llvm::Value* zero_d = cf(0.0);
  llvm::Value* one_d  = cf(1.0);
  llvm::Value* dt     = ci(interval);

  llvm::Function* fn = b.GetInsertBlock()->getParent();

  llvm::BasicBlock* bb_init_chk   = llvm::BasicBlock::Create(ctx, "rc_init_chk",   fn);
  llvm::BasicBlock* bb_do_init    = llvm::BasicBlock::Create(ctx, "rc_do_init",    fn);
  llvm::BasicBlock* bb_post_init  = llvm::BasicBlock::Create(ctx, "rc_post_init",  fn);
  llvm::BasicBlock* bb_emit_phase = llvm::BasicBlock::Create(ctx, "rc_emit_phase", fn);
  llvm::BasicBlock* bb_loop_hdr   = llvm::BasicBlock::Create(ctx, "rc_loop_hdr",   fn);
  llvm::BasicBlock* bb_loop_body  = llvm::BasicBlock::Create(ctx, "rc_loop_body",  fn);
  llvm::BasicBlock* bb_loop_exit  = llvm::BasicBlock::Create(ctx, "rc_loop_exit",  fn);
  llvm::BasicBlock* bb_grid_chk   = llvm::BasicBlock::Create(ctx, "rc_grid_chk",   fn);
  llvm::BasicBlock* bb_grid_emit  = llvm::BasicBlock::Create(ctx, "rc_grid_emit",  fn);
  llvm::BasicBlock* bb_step_done  = llvm::BasicBlock::Create(ctx, "rc_step_done",  fn);

  // -------------------------------------------------------------------------
  // Branch on initialized flag.
  // -------------------------------------------------------------------------
  llvm::Value* init_d = ld_d(INIT, "rc_init");
  b.CreateCondBr(b.CreateFCmpOEQ(init_d, zero_d, "rc_not_init"),
                 bb_init_chk, bb_emit_phase);

  // -------------------------------------------------------------------------
  // bb_init_chk: compute next_emit from t (and optional t0); set last_value=v.
  // -------------------------------------------------------------------------
  b.SetInsertPoint(bb_init_chk);
  {
    llvm::Value* ne_init = nullptr;
    if (t0_set) {
      // if (t < t0) next_emit = t0
      // else        k = (t - t0) / dt; next_emit = t0 + (k + (snap_first?0:1))*dt
      llvm::Value* t0_v = ci(t0);
      llvm::Value* lt   = b.CreateICmpSLT(t, t0_v, "rc_t_lt_t0");

      llvm::Value* dt_diff = b.CreateSub(t, t0_v, "rc_dt_diff");
      llvm::Value* k       = b.CreateSDiv(dt_diff, dt, "rc_k");
      llvm::Value* k_eff   = snap_first
                                ? k
                                : b.CreateAdd(k, ci(1), "rc_k_eff");
      llvm::Value* ne_else = b.CreateAdd(
          t0_v, b.CreateMul(k_eff, dt, "rc_kdt"), "rc_ne_else");

      ne_init = b.CreateSelect(lt, t0_v, ne_else, "rc_ne_init");
    } else {
      // next_emit = t + dt
      ne_init = b.CreateAdd(t, dt, "rc_ne_init");
    }
    st_d(NEXT_EMIT, b.CreateBitCast(ne_init, f64, "rc_ne_d"));
    st_d(LAST_V, v);
    st_d(INIT, one_d);
  }
  b.CreateBr(bb_do_init);

  // -------------------------------------------------------------------------
  // bb_do_init: if !snap_first, this message is consumed without emit.
  // If snap_first, fall through to the normal emit phase.
  // -------------------------------------------------------------------------
  b.SetInsertPoint(bb_do_init);
  if (snap_first) {
    b.CreateBr(bb_post_init);
  } else {
    b.CreateBr(bb_step_done);
  }

  // -------------------------------------------------------------------------
  // bb_post_init / bb_emit_phase: snap_first updates last_value before the
  // grid-fill loop. Already-init paths skip straight to bb_emit_phase.
  // -------------------------------------------------------------------------
  b.SetInsertPoint(bb_post_init);
  if (snap_first) {
    st_d(LAST_V, v);
  }
  b.CreateBr(bb_emit_phase);

  b.SetInsertPoint(bb_emit_phase);
  if (snap_first) {
    // Already-init path also overwrites last_value with current msg before
    // the grid-fill loop, matching FE.
    st_d(LAST_V, v);
  }
  b.CreateBr(bb_loop_hdr);

  // -------------------------------------------------------------------------
  // bb_loop_hdr / body: while (next_emit < t) emit(next_emit, last_value).
  // -------------------------------------------------------------------------
  b.SetInsertPoint(bb_loop_hdr);
  {
    llvm::Value* ne_i = b.CreateBitCast(ld_d(NEXT_EMIT, "rc_ne_l"), i64, "rc_nei_l");
    llvm::Value* lt   = b.CreateICmpSLT(ne_i, t, "rc_ne_lt_t");
    b.CreateCondBr(lt, bb_loop_body, bb_loop_exit);
  }

  b.SetInsertPoint(bb_loop_body);
  {
    llvm::Value* ne_i = b.CreateBitCast(ld_d(NEXT_EMIT, "rc_ne_b"), i64, "rc_nei_b");
    llvm::Value* lv   = ld_d(LAST_V, "rc_lv_b");
    emit_cb(ne_i, lv);
    llvm::Value* ne_adv = b.CreateAdd(ne_i, dt, "rc_ne_adv");
    st_d(NEXT_EMIT, b.CreateBitCast(ne_adv, f64, "rc_ne_adv_d"));
  }
  b.CreateBr(bb_loop_hdr);

  // -------------------------------------------------------------------------
  // bb_loop_exit: check if next_emit == t to do the on-grid emission.
  // -------------------------------------------------------------------------
  b.SetInsertPoint(bb_loop_exit);
  b.CreateBr(bb_grid_chk);

  b.SetInsertPoint(bb_grid_chk);
  {
    llvm::Value* ne_i = b.CreateBitCast(ld_d(NEXT_EMIT, "rc_ne_g"), i64, "rc_nei_g");
    llvm::Value* eq   = b.CreateICmpEQ(ne_i, t, "rc_ne_eq_t");
    b.CreateCondBr(eq, bb_grid_emit, bb_step_done);
  }

  b.SetInsertPoint(bb_grid_emit);
  {
    // Emit (t, v) — uses the current message's value, not last_value.
    emit_cb(t, v);
    llvm::Value* ne_adv = b.CreateAdd(t, dt, "rc_ne_g_adv");
    st_d(NEXT_EMIT, b.CreateBitCast(ne_adv, f64, "rc_ne_g_d"));
  }
  b.CreateBr(bb_step_done);

  // -------------------------------------------------------------------------
  // bb_step_done: write last_value = v (FE always does this on the trailing
  // path), then leave the IRBuilder positioned for the caller.
  // -------------------------------------------------------------------------
  b.SetInsertPoint(bb_step_done);
  st_d(LAST_V, v);
}

}  // namespace rtbot::jit::emit
