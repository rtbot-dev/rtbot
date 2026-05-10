// Join.cpp
//
// IR emission for Join<N> push and try_sync. Mirrors JoinStage<N> from
// libs/compiled/include/rtbot/compiled/JoinStage.h exactly.
//
// State layout (N * 130 doubles at state_offset):
//   For each port p in [0, N):
//     [state_offset + p*130 + 0..63]   times[64]   — i64 timestamps bit-cast as double
//     [state_offset + p*130 + 64..127] values[64]  — double values
//     [state_offset + p*130 + 128]     head        — size_t stored as double (UIToFP/FPToUI)
//     [state_offset + p*130 + 129]     size        — size_t stored as double (UIToFP/FPToUI)

#include "rtbot/compiled/jit/emit/Join.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

namespace {

// Per-port slot sizes.
static constexpr std::size_t kPortStride   = 130;  // doubles per port
static constexpr std::size_t kTimesOffset  = 0;    // relative to port base
static constexpr std::size_t kValuesOffset = 64;   // relative to port base
static constexpr std::size_t kHeadOffset   = 128;  // relative to port base
static constexpr std::size_t kSizeOffset   = 129;  // relative to port base

// Compute the absolute state-buffer offset for a field in a given port.
std::size_t port_slot(std::size_t state_offset, std::size_t port, std::size_t field_offset) {
  return state_offset + port * kPortStride + field_offset;
}

// Pop the front element from port `port` (advance head, decrement size).
// Emits straight-line IR in the current basic block. No new blocks created.
void emit_pop_front(IrEmissionContext& ec, std::size_t state_offset, std::size_t port) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  const std::size_t cap_val = kJitJoinPortCapacity;

  llvm::Value* head_ptr = ec.state_gep(port_slot(state_offset, port, kHeadOffset));
  llvm::Value* size_ptr = ec.state_gep(port_slot(state_offset, port, kSizeOffset));

  llvm::Value* head_d = b.CreateLoad(f64, head_ptr, "jpop_head_d");
  llvm::Value* size_d = b.CreateLoad(f64, size_ptr, "jpop_size_d");

  llvm::Value* head_i = b.CreateFPToUI(head_d, i64, "jpop_head_i");
  llvm::Value* size_i = b.CreateFPToUI(size_d, i64, "jpop_size_i");

  llvm::Value* cap  = llvm::ConstantInt::get(i64, static_cast<int64_t>(cap_val));
  llvm::Value* one  = llvm::ConstantInt::get(i64, 1);

  // head = (head + 1) % cap
  llvm::Value* head1 = b.CreateAdd(head_i, one, "jpop_head1");
  llvm::Value* new_head_i = b.CreateURem(head1, cap, "jpop_new_head");
  // size = size - 1
  llvm::Value* new_size_i = b.CreateSub(size_i, one, "jpop_new_size");

  b.CreateStore(b.CreateUIToFP(new_head_i, f64, "jpop_head_d2"), head_ptr);
  b.CreateStore(b.CreateUIToFP(new_size_i, f64, "jpop_size_d2"), size_ptr);
}

// Load the front timestamp (i64) of port `port`. Straight-line, no new blocks.
llvm::Value* emit_front_time(IrEmissionContext& ec, std::size_t state_offset,
                              std::size_t port) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  llvm::Value* head_ptr = ec.state_gep(port_slot(state_offset, port, kHeadOffset));
  llvm::Value* head_d   = b.CreateLoad(f64, head_ptr, "jft_head_d");
  llvm::Value* head_i   = b.CreateFPToUI(head_d, i64, "jft_head_i");

  llvm::Value* times_base = ec.state_gep(port_slot(state_offset, port, kTimesOffset));
  llvm::Value* t_ptr      = b.CreateGEP(f64, times_base, head_i, "jft_tptr");
  llvm::Value* t_d        = b.CreateLoad(f64, t_ptr, "jft_t_d");
  return b.CreateBitCast(t_d, i64, "jft_t_i");
}

// Load the front value (double) of port `port`. Straight-line, no new blocks.
llvm::Value* emit_front_value(IrEmissionContext& ec, std::size_t state_offset,
                               std::size_t port) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  llvm::Value* head_ptr = ec.state_gep(port_slot(state_offset, port, kHeadOffset));
  llvm::Value* head_d   = b.CreateLoad(f64, head_ptr, "jfv_head_d");
  llvm::Value* head_i   = b.CreateFPToUI(head_d, i64, "jfv_head_i");

  llvm::Value* vals_base = ec.state_gep(port_slot(state_offset, port, kValuesOffset));
  llvm::Value* v_ptr     = b.CreateGEP(f64, vals_base, head_i, "jfv_vptr");
  return b.CreateLoad(f64, v_ptr, "jfv_v");
}

}  // namespace

// ---------------------------------------------------------------------------
// Public re-exports for Demux/Mux emitters that share the per-port ring layout.
// ---------------------------------------------------------------------------
void emit_port_pop_front(IrEmissionContext& ec, std::size_t state_offset,
                         std::size_t port) {
  emit_pop_front(ec, state_offset, port);
}

llvm::Value* emit_port_size(IrEmissionContext& ec, std::size_t state_offset,
                            std::size_t port) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Value* size_ptr = ec.state_gep(port_slot(state_offset, port, kSizeOffset));
  llvm::Value* size_d   = b.CreateLoad(f64, size_ptr, "psz_d");
  return b.CreateFPToUI(size_d, i64, "psz_i");
}

llvm::Value* emit_port_front_time(IrEmissionContext& ec, std::size_t state_offset,
                                  std::size_t port) {
  return emit_front_time(ec, state_offset, port);
}

llvm::Value* emit_port_front_value(IrEmissionContext& ec, std::size_t state_offset,
                                   std::size_t port) {
  return emit_front_value(ec, state_offset, port);
}

// ---------------------------------------------------------------------------
// emit_join_push
// ---------------------------------------------------------------------------
void emit_join_push(IrEmissionContext& ec, std::size_t state_offset, std::size_t /*N*/,
                    std::size_t port, llvm::Value* t, llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  const std::size_t cap_val = kJitJoinPortCapacity;

  llvm::Value* cap64 = llvm::ConstantInt::get(i64, static_cast<int64_t>(cap_val));
  llvm::Value* one64 = llvm::ConstantInt::get(i64, 1);

  llvm::Value* head_ptr = ec.state_gep(port_slot(state_offset, port, kHeadOffset));
  llvm::Value* size_ptr = ec.state_gep(port_slot(state_offset, port, kSizeOffset));

  // Load head and size.
  llvm::Value* head_d = b.CreateLoad(f64, head_ptr, "jp_head_d");
  llvm::Value* size_d = b.CreateLoad(f64, size_ptr, "jp_size_d");

  llvm::Value* head_i = b.CreateFPToUI(head_d, i64, "jp_head_i");
  llvm::Value* size_i = b.CreateFPToUI(size_d, i64, "jp_size_i");

  // Check if size == cap (ring is full — must drop oldest).
  llvm::Value* is_full = b.CreateICmpEQ(size_i, cap64, "jp_is_full");

  llvm::Function* fn     = b.GetInsertBlock()->getParent();
  llvm::BasicBlock* bb_before = b.GetInsertBlock();  // captured before branch
  llvm::BasicBlock* bb_drop   = llvm::BasicBlock::Create(ctx, "jp_drop_oldest", fn);
  llvm::BasicBlock* bb_write  = llvm::BasicBlock::Create(ctx, "jp_write",       fn);

  b.CreateCondBr(is_full, bb_drop, bb_write);

  // --- drop_oldest: head = (head + 1) % cap; size-- -------------------------
  b.SetInsertPoint(bb_drop);
  llvm::Value* head1           = b.CreateAdd(head_i, one64, "jp_head1");
  llvm::Value* new_head_drop   = b.CreateURem(head1, cap64, "jp_new_head");
  llvm::Value* new_size_drop   = b.CreateSub(size_i, one64, "jp_size_dec");
  // We update state here so that subsequent reads see the new head.
  // (The write block will reload using the PHI'd values, not the state.)
  b.CreateBr(bb_write);

  // --- write: idx = (head + size) % cap; times[idx] = t; values[idx] = v; size++ ---
  b.SetInsertPoint(bb_write);

  // PHI nodes carry the effective head and size into the write block.
  llvm::PHINode* phi_head = b.CreatePHI(i64, 2, "jp_phi_head");
  phi_head->addIncoming(new_head_drop, bb_drop);
  phi_head->addIncoming(head_i,        bb_before);

  llvm::PHINode* phi_size = b.CreatePHI(i64, 2, "jp_phi_size");
  phi_size->addIncoming(new_size_drop, bb_drop);
  phi_size->addIncoming(size_i,        bb_before);

  // Write the (possibly updated) head back to state.
  b.CreateStore(b.CreateUIToFP(phi_head, f64, "jp_head_wb"), head_ptr);

  // Compute write index: idx = (head + size) % cap.
  llvm::Value* head_plus_size = b.CreateAdd(phi_head, phi_size, "jp_hps");
  llvm::Value* idx            = b.CreateURem(head_plus_size, cap64, "jp_idx");

  // Store t as bit-cast double into times ring.
  llvm::Value* times_base = ec.state_gep(port_slot(state_offset, port, kTimesOffset));
  llvm::Value* t_ptr      = b.CreateGEP(f64, times_base, idx, "jp_t_ptr");
  b.CreateStore(b.CreateBitCast(t, f64, "jp_t_bc"), t_ptr);

  // Store v into values ring.
  llvm::Value* vals_base = ec.state_gep(port_slot(state_offset, port, kValuesOffset));
  llvm::Value* v_ptr     = b.CreateGEP(f64, vals_base, idx, "jp_v_ptr");
  b.CreateStore(v, v_ptr);

  // size = phi_size + 1 (net: +1 overall, or +0 if we dropped and re-added)
  llvm::Value* new_size_final = b.CreateAdd(phi_size, one64, "jp_size_inc");
  b.CreateStore(b.CreateUIToFP(new_size_final, f64, "jp_size_d3"), size_ptr);
}

// ---------------------------------------------------------------------------
// emit_join_try_sync
// ---------------------------------------------------------------------------
JoinSyncOutput emit_join_try_sync(IrEmissionContext& ec, std::size_t state_offset,
                                  std::size_t N) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i32 = llvm::Type::getInt32Ty(ctx);
  llvm::Type* i1  = llvm::Type::getInt1Ty(ctx);

  llvm::Value* zero_i64 = llvm::ConstantInt::get(i64, 0);
  llvm::Value* zero_f64 = llvm::ConstantFP::get(f64, 0.0);
  llvm::Value* one_i32  = llvm::ConstantInt::get(i32, 1);

  const int max_iters = static_cast<int>(2 * kJitJoinPortCapacity);  // 128

  llvm::Function* fn = b.GetInsertBlock()->getParent();
  llvm::BasicBlock* bb_entry = b.GetInsertBlock();

  llvm::BasicBlock* bb_loop_hdr  = llvm::BasicBlock::Create(ctx, "jts_loop_hdr",  fn);
  llvm::BasicBlock* bb_loop_body = llvm::BasicBlock::Create(ctx, "jts_loop_body", fn);
  llvm::BasicBlock* bb_synced    = llvm::BasicBlock::Create(ctx, "jts_synced",    fn);
  llvm::BasicBlock* bb_drop_mins = llvm::BasicBlock::Create(ctx, "jts_drop_mins", fn);
  llvm::BasicBlock* bb_no_sync   = llvm::BasicBlock::Create(ctx, "jts_no_sync",   fn);
  llvm::BasicBlock* bb_exit      = llvm::BasicBlock::Create(ctx, "jts_exit",      fn);

  // Initialise iter_count = 0 in the entry block and jump to loop header.
  b.CreateBr(bb_loop_hdr);

  // ---------------------------------------------------------------------------
  // Loop header: check iter_count < max_iters.
  // ---------------------------------------------------------------------------
  b.SetInsertPoint(bb_loop_hdr);

  llvm::PHINode* phi_iter = b.CreatePHI(i32, 2, "jts_iter");
  phi_iter->addIncoming(llvm::ConstantInt::get(i32, 0), bb_entry);

  llvm::Value* iter_lt_max = b.CreateICmpSLT(
      phi_iter, llvm::ConstantInt::get(i32, max_iters), "jts_iter_lt");
  b.CreateCondBr(iter_lt_max, bb_loop_body, bb_no_sync);

  // ---------------------------------------------------------------------------
  // Loop body: check all sizes > 0, find min/max, decide sync or drop.
  // ---------------------------------------------------------------------------
  b.SetInsertPoint(bb_loop_body);

  // --- (1) All ports must have size > 0; if any is 0, jump to no_sync -------
  for (std::size_t p = 0; p < N; ++p) {
    llvm::Value* size_ptr = ec.state_gep(port_slot(state_offset, p, kSizeOffset));
    llvm::Value* size_d   = b.CreateLoad(f64, size_ptr, "jts_sz_d");
    llvm::Value* size_i   = b.CreateFPToUI(size_d, i64, "jts_sz_i");
    llvm::Value* is_empty = b.CreateICmpEQ(size_i, zero_i64, "jts_empty");

    // If empty, go to no_sync; otherwise fall through.
    llvm::BasicBlock* bb_cont = llvm::BasicBlock::Create(ctx, "jts_chk_cont", fn);
    b.CreateCondBr(is_empty, bb_no_sync, bb_cont);
    b.SetInsertPoint(bb_cont);
  }

  // --- (2) Load front times; compute min_t and max_t (unrolled) -------------
  llvm::Value* ft0   = emit_front_time(ec, state_offset, 0);
  llvm::Value* min_t = ft0;
  llvm::Value* max_t = ft0;

  for (std::size_t i = 1; i < N; ++i) {
    llvm::Value* fti = emit_front_time(ec, state_offset, i);
    // min_t = fti < min_t ? fti : min_t
    llvm::Value* lt = b.CreateICmpSLT(fti, min_t, "jts_lt");
    min_t = b.CreateSelect(lt, fti, min_t, "jts_min");
    // max_t = fti > max_t ? fti : max_t
    llvm::Value* gt = b.CreateICmpSGT(fti, max_t, "jts_gt");
    max_t = b.CreateSelect(gt, fti, max_t, "jts_max");
  }

  // --- (3) If min_t == max_t, we're synced -----------------------------------
  llvm::Value* is_synced = b.CreateICmpEQ(min_t, max_t, "jts_synced");
  b.CreateCondBr(is_synced, bb_synced, bb_drop_mins);

  // ---------------------------------------------------------------------------
  // drop_mins: pop front from each port whose front_time == min_t.
  // ---------------------------------------------------------------------------
  b.SetInsertPoint(bb_drop_mins);

  for (std::size_t p = 0; p < N; ++p) {
    llvm::Value* size_ptr = ec.state_gep(port_slot(state_offset, p, kSizeOffset));
    llvm::Value* size_d   = b.CreateLoad(f64, size_ptr, "jts_dm_sz_d");
    llvm::Value* size_i   = b.CreateFPToUI(size_d, i64, "jts_dm_sz_i");
    llvm::Value* has_entry = b.CreateICmpUGT(size_i, zero_i64, "jts_dm_has");

    llvm::Value* ftp         = emit_front_time(ec, state_offset, p);
    llvm::Value* ft_eq_min   = b.CreateICmpEQ(ftp, min_t, "jts_dm_eq");
    llvm::Value* should_pop  = b.CreateAnd(has_entry, ft_eq_min, "jts_dm_pop");

    llvm::BasicBlock* bb_do_pop = llvm::BasicBlock::Create(ctx, "jts_do_pop", fn);
    llvm::BasicBlock* bb_skip   = llvm::BasicBlock::Create(ctx, "jts_skip_pop", fn);

    b.CreateCondBr(should_pop, bb_do_pop, bb_skip);

    b.SetInsertPoint(bb_do_pop);
    emit_pop_front(ec, state_offset, p);
    b.CreateBr(bb_skip);

    b.SetInsertPoint(bb_skip);
  }

  // Increment iter_count and jump back to loop header.
  llvm::Value* iter1 = b.CreateAdd(phi_iter, one_i32, "jts_iter1");
  phi_iter->addIncoming(iter1, b.GetInsertBlock());
  b.CreateBr(bb_loop_hdr);

  // ---------------------------------------------------------------------------
  // synced: collect values, pop each port, jump to exit.
  // ---------------------------------------------------------------------------
  b.SetInsertPoint(bb_synced);

  llvm::Value* sync_out_t = min_t;  // already i64
  std::vector<llvm::Value*> sync_out_vs;
  sync_out_vs.reserve(N);

  for (std::size_t p = 0; p < N; ++p) {
    sync_out_vs.push_back(emit_front_value(ec, state_offset, p));
    emit_pop_front(ec, state_offset, p);
  }

  llvm::BasicBlock* bb_synced_end = b.GetInsertBlock();
  b.CreateBr(bb_exit);

  // ---------------------------------------------------------------------------
  // no_sync: supply default values, jump to exit.
  // ---------------------------------------------------------------------------
  b.SetInsertPoint(bb_no_sync);
  llvm::BasicBlock* bb_no_sync_end = b.GetInsertBlock();
  b.CreateBr(bb_exit);

  // ---------------------------------------------------------------------------
  // exit: PHI for sync_flag, out_t, and each out_v.
  // ---------------------------------------------------------------------------
  b.SetInsertPoint(bb_exit);

  llvm::PHINode* phi_flag = b.CreatePHI(i1, 2, "jts_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_synced_end);
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_no_sync_end);

  llvm::PHINode* phi_out_t = b.CreatePHI(i64, 2, "jts_phi_t");
  phi_out_t->addIncoming(sync_out_t, bb_synced_end);
  phi_out_t->addIncoming(zero_i64,   bb_no_sync_end);

  std::vector<llvm::Value*> phi_out_vs;
  phi_out_vs.reserve(N);
  for (std::size_t p = 0; p < N; ++p) {
    llvm::PHINode* phi_v = b.CreatePHI(f64, 2, "jts_phi_v");
    phi_v->addIncoming(sync_out_vs[p], bb_synced_end);
    phi_v->addIncoming(zero_f64,       bb_no_sync_end);
    phi_out_vs.push_back(phi_v);
  }

  return JoinSyncOutput{phi_flag, phi_out_t, std::move(phi_out_vs)};
}

}  // namespace rtbot::jit::emit
