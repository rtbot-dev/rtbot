// Mux.cpp
//
// IR emission for Multiplexer<N>. Mirrors FE Multiplexer::process_data
// semantics:
//   1. If all data ports empty, bail.
//   2. Sync the N control queues (drop mismatched-older fronts).
//   3. Find unique control port whose front.value is true at the synced ctrl
//      time (find_port_to_emit). If multiple or none, port_to_emit = -1.
//   4. If port_to_emit >= 0:
//        - For each data port i:
//            if i == port_to_emit && data[i].t == ctrl.t: emit, pop data[i]
//                  -> message_found = true
//            elif i == port_to_emit && ctrl.t < data[i].t: keep data[i]
//                  -> message_found = true (controls advance regardless)
//            elif data[i].t <= ctrl.t: pop data[i]   // drop stale
//        - if message_found: pop all controls.
//   5. Else (port_to_emit < 0):
//        - For each data port i: if !empty && data[i].t <= ctrl.t, pop it.
//        - Pop all controls.
//
// State layout: 2*N Join-style PortBuffer<64> ring buffers. Data ports occupy
// indices [0, N); control ports occupy [N, 2N).
//
// One dispatch step per call. The FE outer while(true) is omitted; rtbot's
// monotonic-time invariant means at most one dispatch is dispatchable per
// push under typical usage.

#include "rtbot/compiled/jit/emit/Mux.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

#include "rtbot/compiled/jit/emit/Join.h"

namespace rtbot::jit::emit {

void emit_mux_push_data(IrEmissionContext& ec, std::size_t state_offset,
                        std::size_t /*N*/, std::size_t port,
                        llvm::Value* t, llvm::Value* v) {
  emit_join_push(ec, state_offset, /*N (unused)*/ 0, /*port=*/port, t, v);
}

void emit_mux_push_control(IrEmissionContext& ec, std::size_t state_offset,
                           std::size_t N, std::size_t ctrl_idx,
                           llvm::Value* t, llvm::Value* v) {
  emit_join_push(ec, state_offset, /*N (unused)*/ 0, /*port=*/N + ctrl_idx, t, v);
}

MuxTrySyncOutput emit_mux_try_sync(IrEmissionContext& ec, std::size_t state_offset,
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

  llvm::BasicBlock* bb_after_dempty = llvm::BasicBlock::Create(ctx, "mux_after_dempty", fn);
  llvm::BasicBlock* bb_csync_hdr    = llvm::BasicBlock::Create(ctx, "mux_csync_hdr",    fn);
  llvm::BasicBlock* bb_csync_body   = llvm::BasicBlock::Create(ctx, "mux_csync_body",   fn);
  llvm::BasicBlock* bb_csync_drop   = llvm::BasicBlock::Create(ctx, "mux_csync_drop",   fn);
  llvm::BasicBlock* bb_csync_done   = llvm::BasicBlock::Create(ctx, "mux_csync_done",   fn);
  llvm::BasicBlock* bb_dispatch     = llvm::BasicBlock::Create(ctx, "mux_dispatch",     fn);
  llvm::BasicBlock* bb_no_match     = llvm::BasicBlock::Create(ctx, "mux_no_match",     fn);
  llvm::BasicBlock* bb_no_sync      = llvm::BasicBlock::Create(ctx, "mux_no_sync",      fn);
  llvm::BasicBlock* bb_synced       = llvm::BasicBlock::Create(ctx, "mux_synced",       fn);
  llvm::BasicBlock* bb_exit         = llvm::BasicBlock::Create(ctx, "mux_exit",         fn);

  // ============================================================
  // Phase 0: bail if every data port is empty.
  // ============================================================
  llvm::Value* all_data_empty = llvm::ConstantInt::getTrue(ctx);
  for (std::size_t i = 0; i < N; ++i) {
    llvm::Value* sz = emit_port_size(ec, state_offset, /*port=*/i);
    llvm::Value* is_empty = b.CreateICmpEQ(sz, zero_i64, "mux_d_empty");
    all_data_empty = b.CreateAnd(all_data_empty, is_empty, "mux_all_d_empty");
  }
  b.CreateCondBr(all_data_empty, bb_no_sync, bb_after_dempty);

  // ============================================================
  // Phase 1: sync controls (ports N..2N).
  // ============================================================
  b.SetInsertPoint(bb_after_dempty);
  b.CreateBr(bb_csync_hdr);

  b.SetInsertPoint(bb_csync_hdr);
  llvm::PHINode* phi_iter = b.CreatePHI(i32, 2, "mux_citer");
  phi_iter->addIncoming(llvm::ConstantInt::get(i32, 0), bb_after_dempty);
  llvm::Value* iter_lt_max = b.CreateICmpSLT(
      phi_iter, llvm::ConstantInt::get(i32, max_iters), "mux_citer_lt");
  b.CreateCondBr(iter_lt_max, bb_csync_body, bb_no_sync);

  b.SetInsertPoint(bb_csync_body);
  for (std::size_t i = 0; i < N; ++i) {
    llvm::Value* sz = emit_port_size(ec, state_offset, N + i);
    llvm::Value* is_empty = b.CreateICmpEQ(sz, zero_i64, "mux_cempty");
    llvm::BasicBlock* bb_cont = llvm::BasicBlock::Create(ctx, "mux_cchk_cont", fn);
    b.CreateCondBr(is_empty, bb_no_sync, bb_cont);
    b.SetInsertPoint(bb_cont);
  }

  llvm::Value* cmin_t = nullptr;
  llvm::Value* cmax_t = nullptr;
  if (N >= 1) {
    cmin_t = emit_port_front_time(ec, state_offset, N);
    cmax_t = cmin_t;
    for (std::size_t i = 1; i < N; ++i) {
      llvm::Value* fti = emit_port_front_time(ec, state_offset, N + i);
      llvm::Value* lt = b.CreateICmpSLT(fti, cmin_t, "mux_clt");
      cmin_t = b.CreateSelect(lt, fti, cmin_t, "mux_cmin");
      llvm::Value* gt = b.CreateICmpSGT(fti, cmax_t, "mux_cgt");
      cmax_t = b.CreateSelect(gt, fti, cmax_t, "mux_cmax");
    }
  } else {
    cmin_t = zero_i64;
    cmax_t = zero_i64;
  }

  llvm::Value* csynced = b.CreateICmpEQ(cmin_t, cmax_t, "mux_csync");
  b.CreateCondBr(csynced, bb_csync_done, bb_csync_drop);

  b.SetInsertPoint(bb_csync_drop);
  for (std::size_t i = 0; i < N; ++i) {
    llvm::Value* sz = emit_port_size(ec, state_offset, N + i);
    llvm::Value* has_entry = b.CreateICmpUGT(sz, zero_i64, "mux_chas");
    llvm::Value* fti = emit_port_front_time(ec, state_offset, N + i);
    llvm::Value* eq = b.CreateICmpEQ(fti, cmin_t, "mux_ceq");
    llvm::Value* should_pop = b.CreateAnd(has_entry, eq, "mux_cpop");
    llvm::BasicBlock* bb_pop_yes = llvm::BasicBlock::Create(ctx, "mux_cpop_yes", fn);
    llvm::BasicBlock* bb_pop_no  = llvm::BasicBlock::Create(ctx, "mux_cpop_no",  fn);
    b.CreateCondBr(should_pop, bb_pop_yes, bb_pop_no);
    b.SetInsertPoint(bb_pop_yes);
    emit_port_pop_front(ec, state_offset, N + i);
    b.CreateBr(bb_pop_no);
    b.SetInsertPoint(bb_pop_no);
  }
  llvm::Value* iter1 = b.CreateAdd(phi_iter, one_i32, "mux_citer1");
  phi_iter->addIncoming(iter1, b.GetInsertBlock());
  b.CreateBr(bb_csync_hdr);

  // ============================================================
  // Phase 2: controls all front at cmin_t. Find unique active port.
  // ============================================================
  b.SetInsertPoint(bb_csync_done);
  llvm::Value* ctrl_t = cmin_t;

  // active_count: number of controls with value != 0.0 at cmin_t.
  // selected_port: last index where this is true (i32).
  llvm::Value* active_count = llvm::ConstantInt::get(i32, 0);
  llvm::Value* selected_port = llvm::ConstantInt::get(i32, -1);
  for (std::size_t i = 0; i < N; ++i) {
    llvm::Value* cv = emit_port_front_value(ec, state_offset, N + i);
    llvm::Value* nz = b.CreateFCmpONE(cv, zero_f64, "mux_cnz");
    llvm::Value* incr = b.CreateZExt(nz, i32, "mux_cincr");
    active_count = b.CreateAdd(active_count, incr, "mux_cact");
    llvm::Value* idx = llvm::ConstantInt::get(i32, static_cast<int32_t>(i));
    selected_port = b.CreateSelect(nz, idx, selected_port, "mux_csel");
  }
  llvm::Value* exactly_one = b.CreateICmpEQ(
      active_count, llvm::ConstantInt::get(i32, 1), "mux_one");
  b.CreateCondBr(exactly_one, bb_dispatch, bb_no_match);

  // ============================================================
  // bb_dispatch: process per-data-port logic for selected port.
  // We compute emit_v (PHI'd from per-port branches), and message_found.
  // After processing all ports, conditionally pop controls.
  // ============================================================
  b.SetInsertPoint(bb_dispatch);

  // Track running message_found and the chosen out_v across data ports.
  llvm::Value* msg_found_acc = llvm::ConstantInt::getFalse(ctx);
  llvm::Value* out_v_acc = zero_f64;
  llvm::Value* did_emit_acc = llvm::ConstantInt::getFalse(ctx);

  for (std::size_t i = 0; i < N; ++i) {
    llvm::Value* idx_i = llvm::ConstantInt::get(i32, static_cast<int32_t>(i));
    llvm::Value* is_target = b.CreateICmpEQ(selected_port, idx_i, "mux_dt");

    llvm::Value* sz = emit_port_size(ec, state_offset, /*port=*/i);
    llvm::Value* has_entry = b.CreateICmpUGT(sz, zero_i64, "mux_dh");

    // Process port i in a small inline diamond:
    //   if (!has_entry) skip.
    //   else:
    //     d_t, d_v = front
    //     if (is_target && d_t == ctrl_t): emit + pop. msg_found=true.
    //     elif (is_target && ctrl_t < d_t): keep. msg_found=true.
    //     elif (d_t <= ctrl_t): pop.    // stale on a non-target port
    //
    // We need the loop-carried (msg_found, out_v, did_emit) values to PHI
    // back at the diamond merge.

    llvm::BasicBlock* bb_entry_d   = b.GetInsertBlock();
    llvm::BasicBlock* bb_has       = llvm::BasicBlock::Create(ctx, "mux_dh_yes",  fn);
    llvm::BasicBlock* bb_emit      = llvm::BasicBlock::Create(ctx, "mux_d_emit",  fn);
    llvm::BasicBlock* bb_keep      = llvm::BasicBlock::Create(ctx, "mux_d_keep",  fn);
    llvm::BasicBlock* bb_drop      = llvm::BasicBlock::Create(ctx, "mux_d_drop",  fn);
    llvm::BasicBlock* bb_d_done    = llvm::BasicBlock::Create(ctx, "mux_d_done",  fn);

    b.CreateCondBr(has_entry, bb_has, bb_d_done);

    // bb_has: classify and act.
    b.SetInsertPoint(bb_has);
    llvm::Value* d_t = emit_port_front_time(ec, state_offset, /*port=*/i);
    llvm::Value* d_v = emit_port_front_value(ec, state_offset, /*port=*/i);

    llvm::Value* d_eq    = b.CreateICmpEQ(d_t, ctrl_t, "mux_d_eq");
    llvm::Value* ctrl_lt = b.CreateICmpSLT(ctrl_t, d_t, "mux_d_clt");
    llvm::Value* d_le    = b.CreateICmpSLE(d_t, ctrl_t, "mux_d_le");

    llvm::Value* take_emit = b.CreateAnd(is_target, d_eq,    "mux_d_te");
    llvm::Value* take_keep = b.CreateAnd(is_target, ctrl_lt, "mux_d_tk");
    llvm::Value* take_drop = b.CreateAnd(b.CreateNot(is_target, "mux_d_nt"), d_le,
                                          "mux_d_td");

    llvm::BasicBlock* bb_emit_chk = llvm::BasicBlock::Create(ctx, "mux_d_emit_chk", fn);
    llvm::BasicBlock* bb_keep_chk = llvm::BasicBlock::Create(ctx, "mux_d_keep_chk", fn);
    llvm::BasicBlock* bb_drop_chk = llvm::BasicBlock::Create(ctx, "mux_d_drop_chk", fn);
    llvm::BasicBlock* bb_d_finalize = llvm::BasicBlock::Create(ctx, "mux_d_fin",   fn);

    b.CreateCondBr(take_emit, bb_emit, bb_emit_chk);
    b.SetInsertPoint(bb_emit_chk);
    b.CreateCondBr(take_keep, bb_keep, bb_keep_chk);
    b.SetInsertPoint(bb_keep_chk);
    b.CreateCondBr(take_drop, bb_drop, bb_drop_chk);
    b.SetInsertPoint(bb_drop_chk);
    b.CreateBr(bb_d_finalize);

    // bb_emit: pop port i; record d_v as out_v; mark msg_found, did_emit.
    b.SetInsertPoint(bb_emit);
    emit_port_pop_front(ec, state_offset, /*port=*/i);
    b.CreateBr(bb_d_finalize);

    // bb_keep: do not pop; record msg_found.
    b.SetInsertPoint(bb_keep);
    b.CreateBr(bb_d_finalize);

    // bb_drop: pop port i; do not record any output.
    b.SetInsertPoint(bb_drop);
    emit_port_pop_front(ec, state_offset, /*port=*/i);
    b.CreateBr(bb_d_finalize);

    // bb_d_finalize: PHI msg_found / out_v / did_emit from the four sub-blocks.
    b.SetInsertPoint(bb_d_finalize);
    llvm::PHINode* phi_mf = b.CreatePHI(i1, 4, "mux_d_phi_mf");
    phi_mf->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_emit);
    phi_mf->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_keep);
    phi_mf->addIncoming(msg_found_acc,                    bb_drop);
    phi_mf->addIncoming(msg_found_acc,                    bb_drop_chk);

    llvm::PHINode* phi_de = b.CreatePHI(i1, 4, "mux_d_phi_de");
    phi_de->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_emit);
    phi_de->addIncoming(did_emit_acc,                     bb_keep);
    phi_de->addIncoming(did_emit_acc,                     bb_drop);
    phi_de->addIncoming(did_emit_acc,                     bb_drop_chk);

    llvm::PHINode* phi_ov = b.CreatePHI(f64, 4, "mux_d_phi_ov");
    phi_ov->addIncoming(d_v,         bb_emit);
    phi_ov->addIncoming(out_v_acc,   bb_keep);
    phi_ov->addIncoming(out_v_acc,   bb_drop);
    phi_ov->addIncoming(out_v_acc,   bb_drop_chk);

    // Branch out to bb_d_done.
    b.CreateBr(bb_d_done);

    // bb_d_done: merge with the !has_entry path.
    b.SetInsertPoint(bb_d_done);
    llvm::PHINode* phi_mf2 = b.CreatePHI(i1, 2, "mux_d_phi_mf2");
    phi_mf2->addIncoming(phi_mf,        bb_d_finalize);
    phi_mf2->addIncoming(msg_found_acc, bb_entry_d);

    llvm::PHINode* phi_de2 = b.CreatePHI(i1, 2, "mux_d_phi_de2");
    phi_de2->addIncoming(phi_de,        bb_d_finalize);
    phi_de2->addIncoming(did_emit_acc,  bb_entry_d);

    llvm::PHINode* phi_ov2 = b.CreatePHI(f64, 2, "mux_d_phi_ov2");
    phi_ov2->addIncoming(phi_ov,        bb_d_finalize);
    phi_ov2->addIncoming(out_v_acc,     bb_entry_d);

    msg_found_acc = phi_mf2;
    did_emit_acc  = phi_de2;
    out_v_acc     = phi_ov2;
  }

  // After the per-port loop: if message_found, pop all controls.
  llvm::BasicBlock* bb_pop_ctrls = llvm::BasicBlock::Create(ctx, "mux_pop_ctrls", fn);
  llvm::BasicBlock* bb_after_disp = llvm::BasicBlock::Create(ctx, "mux_after_disp", fn);
  b.CreateCondBr(msg_found_acc, bb_pop_ctrls, bb_after_disp);

  b.SetInsertPoint(bb_pop_ctrls);
  for (std::size_t i = 0; i < N; ++i) {
    emit_port_pop_front(ec, state_offset, N + i);
  }
  b.CreateBr(bb_after_disp);

  b.SetInsertPoint(bb_after_disp);
  // If did_emit, jump to bb_synced; else to bb_no_sync.
  b.CreateCondBr(did_emit_acc, bb_synced, bb_no_sync);

  llvm::Value* synced_v_from_disp = out_v_acc;

  // ============================================================
  // bb_no_match: drop stale data fronts and pop all controls.
  // ============================================================
  b.SetInsertPoint(bb_no_match);
  for (std::size_t i = 0; i < N; ++i) {
    llvm::Value* sz = emit_port_size(ec, state_offset, /*port=*/i);
    llvm::Value* has_entry = b.CreateICmpUGT(sz, zero_i64, "mux_nm_h");
    llvm::Value* d_t = emit_port_front_time(ec, state_offset, /*port=*/i);
    llvm::Value* d_le = b.CreateICmpSLE(d_t, ctrl_t, "mux_nm_le");
    llvm::Value* should_pop = b.CreateAnd(has_entry, d_le, "mux_nm_pop");
    llvm::BasicBlock* bb_p = llvm::BasicBlock::Create(ctx, "mux_nm_pop_yes", fn);
    llvm::BasicBlock* bb_n = llvm::BasicBlock::Create(ctx, "mux_nm_pop_no",  fn);
    b.CreateCondBr(should_pop, bb_p, bb_n);
    b.SetInsertPoint(bb_p);
    emit_port_pop_front(ec, state_offset, /*port=*/i);
    b.CreateBr(bb_n);
    b.SetInsertPoint(bb_n);
  }
  // Pop all controls.
  for (std::size_t i = 0; i < N; ++i) {
    emit_port_pop_front(ec, state_offset, N + i);
  }
  b.CreateBr(bb_no_sync);

  // ============================================================
  // bb_synced and bb_no_sync converge at bb_exit.
  // ============================================================
  b.SetInsertPoint(bb_synced);
  llvm::BasicBlock* bb_synced_end = b.GetInsertBlock();
  b.CreateBr(bb_exit);

  b.SetInsertPoint(bb_no_sync);
  llvm::BasicBlock* bb_no_sync_end = b.GetInsertBlock();
  b.CreateBr(bb_exit);

  // ============================================================
  // Exit: PHI sync_flag, out_t, out_v.
  // ============================================================
  b.SetInsertPoint(bb_exit);

  llvm::PHINode* phi_flag = b.CreatePHI(i1, 2, "mux_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_synced_end);
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_no_sync_end);

  llvm::PHINode* phi_out_t = b.CreatePHI(i64, 2, "mux_phi_t");
  phi_out_t->addIncoming(ctrl_t,    bb_synced_end);
  phi_out_t->addIncoming(zero_i64,  bb_no_sync_end);

  llvm::PHINode* phi_out_v = b.CreatePHI(f64, 2, "mux_phi_v");
  phi_out_v->addIncoming(synced_v_from_disp, bb_synced_end);
  phi_out_v->addIncoming(zero_f64,           bb_no_sync_end);

  return MuxTrySyncOutput{phi_flag, phi_out_t, phi_out_v};
}

}  // namespace rtbot::jit::emit
