// Demux.cpp
//
// IR emission for Demultiplexer<N> push and try_sync. Mirrors FE
// Demultiplexer::process_data semantics:
//   1. Sync the N control queues (drop mismatched-older fronts) until all
//      control fronts share a timestamp OR any control becomes empty.
//   2. If controls aren't synced, bail.
//   3. If data queue is empty, bail.
//   4. If data.t == ctrl.t: for each control i, if ctrl[i].value, emit
//      data value to output port i. Pop data and all controls.
//   5. If data.t < ctrl.t: pop data (and bail this round).
//   6. If ctrl.t < data.t: pop all controls (and bail this round).
//
// Each segment_fn call performs a single dispatch step. (FE's outer
// while(true) is omitted; rtbot's monotonic-time invariant means at most one
// dispatch is dispatchable per push under typical usage. This matches the
// behavior of the JIT Join emitter which also does one dispatch per push.)
//
// State layout: 1 + N Join-style PortBuffer<64> ring buffers. Data is port 0;
// controls are ports 1..N.

#include "rtbot/compiled/jit/emit/Demux.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

#include "rtbot/compiled/jit/emit/Join.h"

namespace rtbot::jit::emit {

void emit_demux_push_data(IrEmissionContext& ec, std::size_t state_offset,
                          llvm::Value* t, llvm::Value* v) {
  emit_join_push(ec, state_offset, /*N (unused)*/ 0, /*port=*/0, t, v);
}

void emit_demux_push_control(IrEmissionContext& ec, std::size_t state_offset,
                             std::size_t /*N*/, std::size_t ctrl_idx,
                             llvm::Value* t, llvm::Value* v) {
  emit_join_push(ec, state_offset, /*N (unused)*/ 0, /*port=*/1 + ctrl_idx, t, v);
}

DemuxTrySyncOutput emit_demux_try_sync(IrEmissionContext& ec,
                                        std::size_t state_offset,
                                        std::size_t N) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i32 = llvm::Type::getInt32Ty(ctx);
  llvm::Type* i1  = llvm::Type::getInt1Ty(ctx);

  llvm::Value* zero_i64 = llvm::ConstantInt::get(i64, 0);
  llvm::Value* zero_f64 = llvm::ConstantFP::get(f64, 0.0);
  llvm::Value* one_i32  = llvm::ConstantInt::get(i32, 1);

  const int max_iters = static_cast<int>(2 * 64);

  llvm::Function* fn = b.GetInsertBlock()->getParent();
  llvm::BasicBlock* bb_entry = b.GetInsertBlock();

  // Phase 1: sync the N control queues.
  llvm::BasicBlock* bb_csync_hdr  = llvm::BasicBlock::Create(ctx, "dmx_csync_hdr",  fn);
  llvm::BasicBlock* bb_csync_body = llvm::BasicBlock::Create(ctx, "dmx_csync_body", fn);
  llvm::BasicBlock* bb_csync_drop = llvm::BasicBlock::Create(ctx, "dmx_csync_drop", fn);
  llvm::BasicBlock* bb_csync_done = llvm::BasicBlock::Create(ctx, "dmx_csync_done", fn);
  // Phase 2: data-vs-control alignment.
  llvm::BasicBlock* bb_check_data = llvm::BasicBlock::Create(ctx, "dmx_check_data", fn);
  llvm::BasicBlock* bb_match      = llvm::BasicBlock::Create(ctx, "dmx_match",      fn);
  llvm::BasicBlock* bb_no_match   = llvm::BasicBlock::Create(ctx, "dmx_no_match",   fn);
  llvm::BasicBlock* bb_data_lt    = llvm::BasicBlock::Create(ctx, "dmx_data_lt",    fn);
  llvm::BasicBlock* bb_ctrl_lt    = llvm::BasicBlock::Create(ctx, "dmx_ctrl_lt",    fn);
  llvm::BasicBlock* bb_no_sync    = llvm::BasicBlock::Create(ctx, "dmx_no_sync",    fn);
  llvm::BasicBlock* bb_synced     = llvm::BasicBlock::Create(ctx, "dmx_synced",     fn);
  llvm::BasicBlock* bb_exit       = llvm::BasicBlock::Create(ctx, "dmx_exit",       fn);

  // ============================================================
  // Phase 1: sync controls (ports 1..N).
  // ============================================================
  b.CreateBr(bb_csync_hdr);

  // Loop header.
  b.SetInsertPoint(bb_csync_hdr);
  llvm::PHINode* phi_iter = b.CreatePHI(i32, 2, "dmx_citer");
  phi_iter->addIncoming(llvm::ConstantInt::get(i32, 0), bb_entry);
  llvm::Value* iter_lt_max = b.CreateICmpSLT(
      phi_iter, llvm::ConstantInt::get(i32, max_iters), "dmx_citer_lt");
  b.CreateCondBr(iter_lt_max, bb_csync_body, bb_no_sync);

  // Loop body: bail if any control empty; else compute min/max.
  b.SetInsertPoint(bb_csync_body);
  for (std::size_t i = 0; i < N; ++i) {
    llvm::Value* sz = emit_port_size(ec, state_offset, /*port=*/1 + i);
    llvm::Value* is_empty = b.CreateICmpEQ(sz, zero_i64, "dmx_cempty");
    llvm::BasicBlock* bb_cont = llvm::BasicBlock::Create(ctx, "dmx_cchk_cont", fn);
    b.CreateCondBr(is_empty, bb_no_sync, bb_cont);
    b.SetInsertPoint(bb_cont);
  }

  llvm::Value* cmin_t = nullptr;
  llvm::Value* cmax_t = nullptr;
  if (N >= 1) {
    cmin_t = emit_port_front_time(ec, state_offset, 1);
    cmax_t = cmin_t;
    for (std::size_t i = 1; i < N; ++i) {
      llvm::Value* fti = emit_port_front_time(ec, state_offset, 1 + i);
      llvm::Value* lt = b.CreateICmpSLT(fti, cmin_t, "dmx_clt");
      cmin_t = b.CreateSelect(lt, fti, cmin_t, "dmx_cmin");
      llvm::Value* gt = b.CreateICmpSGT(fti, cmax_t, "dmx_cgt");
      cmax_t = b.CreateSelect(gt, fti, cmax_t, "dmx_cmax");
    }
  } else {
    cmin_t = zero_i64;
    cmax_t = zero_i64;
  }

  llvm::Value* csynced = b.CreateICmpEQ(cmin_t, cmax_t, "dmx_csync");
  b.CreateCondBr(csynced, bb_csync_done, bb_csync_drop);

  // Drop: pop control ports whose front_time == cmin_t.
  b.SetInsertPoint(bb_csync_drop);
  for (std::size_t i = 0; i < N; ++i) {
    llvm::Value* sz = emit_port_size(ec, state_offset, 1 + i);
    llvm::Value* has_entry = b.CreateICmpUGT(sz, zero_i64, "dmx_chas");
    llvm::Value* fti = emit_port_front_time(ec, state_offset, 1 + i);
    llvm::Value* eq = b.CreateICmpEQ(fti, cmin_t, "dmx_ceq");
    llvm::Value* should_pop = b.CreateAnd(has_entry, eq, "dmx_cpop");
    llvm::BasicBlock* bb_pop_yes = llvm::BasicBlock::Create(ctx, "dmx_cpop_yes", fn);
    llvm::BasicBlock* bb_pop_no  = llvm::BasicBlock::Create(ctx, "dmx_cpop_no",  fn);
    b.CreateCondBr(should_pop, bb_pop_yes, bb_pop_no);
    b.SetInsertPoint(bb_pop_yes);
    emit_port_pop_front(ec, state_offset, 1 + i);
    b.CreateBr(bb_pop_no);
    b.SetInsertPoint(bb_pop_no);
  }
  llvm::Value* iter1 = b.CreateAdd(phi_iter, one_i32, "dmx_citer1");
  phi_iter->addIncoming(iter1, b.GetInsertBlock());
  b.CreateBr(bb_csync_hdr);

  // ============================================================
  // Phase 2: controls now share `cmin_t` as their front. Compare with data.
  // ============================================================
  b.SetInsertPoint(bb_csync_done);
  // cmin_t (== cmax_t) is the synced control time; reachable only on success.
  llvm::Value* ctrl_t = cmin_t;

  // If data queue is empty, bail.
  llvm::Value* dsz = emit_port_size(ec, state_offset, /*port=*/0);
  llvm::Value* d_empty = b.CreateICmpEQ(dsz, zero_i64, "dmx_dempty");
  b.CreateCondBr(d_empty, bb_no_sync, bb_check_data);

  b.SetInsertPoint(bb_check_data);
  llvm::Value* data_t = emit_port_front_time(ec, state_offset, /*port=*/0);
  llvm::Value* data_eq_ctrl = b.CreateICmpEQ(data_t, ctrl_t, "dmx_deq");
  b.CreateCondBr(data_eq_ctrl, bb_match, bb_no_match);

  b.SetInsertPoint(bb_no_match);
  llvm::Value* data_lt = b.CreateICmpSLT(data_t, ctrl_t, "dmx_dlt");
  b.CreateCondBr(data_lt, bb_data_lt, bb_ctrl_lt);

  // data older: pop data; bail (no emit this round).
  b.SetInsertPoint(bb_data_lt);
  emit_port_pop_front(ec, state_offset, /*port=*/0);
  b.CreateBr(bb_no_sync);

  // ctrl older: pop all controls; bail.
  b.SetInsertPoint(bb_ctrl_lt);
  for (std::size_t i = 0; i < N; ++i) {
    emit_port_pop_front(ec, state_offset, 1 + i);
  }
  b.CreateBr(bb_no_sync);

  // ============================================================
  // Match: collect emit flags from controls, data value, then pop everything.
  // ============================================================
  b.SetInsertPoint(bb_match);

  llvm::Value* data_v = emit_port_front_value(ec, state_offset, /*port=*/0);
  std::vector<llvm::Value*> emits;
  emits.reserve(N);
  for (std::size_t i = 0; i < N; ++i) {
    llvm::Value* cv = emit_port_front_value(ec, state_offset, 1 + i);
    llvm::Value* nz = b.CreateFCmpONE(cv, zero_f64, "dmx_ctrl_nz");
    emits.push_back(nz);
  }
  emit_port_pop_front(ec, state_offset, 0);
  for (std::size_t i = 0; i < N; ++i) {
    emit_port_pop_front(ec, state_offset, 1 + i);
  }
  b.CreateBr(bb_synced);

  b.SetInsertPoint(bb_synced);
  llvm::BasicBlock* bb_synced_end = b.GetInsertBlock();
  b.CreateBr(bb_exit);

  // ============================================================
  // No-sync: nothing to emit.
  // ============================================================
  b.SetInsertPoint(bb_no_sync);
  llvm::BasicBlock* bb_no_sync_end = b.GetInsertBlock();
  b.CreateBr(bb_exit);

  // ============================================================
  // Exit: PHI everything.
  // ============================================================
  b.SetInsertPoint(bb_exit);

  llvm::PHINode* phi_flag = b.CreatePHI(i1, 2, "dmx_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_synced_end);
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_no_sync_end);

  llvm::PHINode* phi_out_t = b.CreatePHI(i64, 2, "dmx_phi_t");
  phi_out_t->addIncoming(ctrl_t,   bb_synced_end);
  phi_out_t->addIncoming(zero_i64, bb_no_sync_end);

  std::vector<llvm::Value*> phi_emits;
  std::vector<llvm::Value*> phi_values;
  phi_emits.reserve(N);
  phi_values.reserve(N);
  for (std::size_t i = 0; i < N; ++i) {
    llvm::PHINode* pe = b.CreatePHI(i1, 2, "dmx_phi_e");
    pe->addIncoming(emits[i], bb_synced_end);
    pe->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_no_sync_end);
    phi_emits.push_back(pe);

    llvm::PHINode* pv = b.CreatePHI(f64, 2, "dmx_phi_v");
    pv->addIncoming(data_v, bb_synced_end);
    pv->addIncoming(zero_f64, bb_no_sync_end);
    phi_values.push_back(pv);
  }

  return DemuxTrySyncOutput{phi_flag, phi_out_t, std::move(phi_emits),
                             std::move(phi_values)};
}

}  // namespace rtbot::jit::emit
