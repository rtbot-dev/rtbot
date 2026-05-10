// TopK.cpp
//
// IR emission for one TopK step. Mirrors FE TopK::process_data:
//   - lower_bound insertion (comparator depends on descending flag)
//   - shift-right on insert
//   - cap occupancy at K (drop worst, always at the back)
//   - emit one record per current row, in best-to-worst order
//
// State layout (1 + K*W doubles at state_offset):
//   [0]                count_d  — number of rows held, in [0, K], stored as double
//   [1 + j*W + k]      lane k of row j (best at j=0)
//
// All loops are in IR (not unrolled) — K can be up to ~100 in practice and
// keeping the IR compact also keeps codegen fast.

#include "rtbot/compiled/jit/emit/TopK.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

llvm::Value* emit_topk(IrEmissionContext& ec,
                       std::size_t state_offset,
                       std::size_t K,
                       std::size_t row_lanes,
                       std::size_t score_index,
                       bool descending,
                       llvm::Value* t,
                       const std::vector<llvm::Value*>& input_lanes,
                       llvm::Value* out_t_arr,
                       llvm::Value* out_v_arr,
                       llvm::Value* out_port_id_arr,
                       std::size_t  num_outputs_per_record,
                       std::size_t  out_port_id) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i32 = llvm::Type::getInt32Ty(ctx);

  auto ci64 = [&](std::int64_t v) { return llvm::ConstantInt::get(i64, v); };
  auto ci32 = [&](std::int32_t v) { return llvm::ConstantInt::get(i32, v); };
  auto cf   = [&](double v)       { return llvm::ConstantFP::get(f64, v);  };

  llvm::Function* fn = b.GetInsertBlock()->getParent();

  // ------------------------------------------------------------------------
  // Step 1: load count_d as double, convert to i64 N.
  // ------------------------------------------------------------------------
  llvm::Value* count_ptr = ec.state_gep(state_offset);
  llvm::Value* rows_base = ec.state_gep(state_offset + 1);

  llvm::Value* count_d = b.CreateLoad(f64, count_ptr, "tk_cnt_d");
  llvm::Value* N       = b.CreateFPToUI(count_d, i64, "tk_N");

  // Score of the new row.
  llvm::Value* new_score = input_lanes[score_index];

  // ------------------------------------------------------------------------
  // Step 2: compute insertion position via lower_bound. Loop j from 0 to N
  // (exclusive), break on first j where !comp(stored, new). On no break,
  // insertion = N.
  //
  // Using an alloca-backed ipos / found pair to avoid PHI gymnastics.
  // ------------------------------------------------------------------------
  llvm::BasicBlock* bb_pre = b.GetInsertBlock();
  llvm::Value* al_ipos  = b.CreateAlloca(i64, nullptr, "tk_ipos");
  llvm::Value* al_found = b.CreateAlloca(b.getInt1Ty(), nullptr, "tk_found");
  b.CreateStore(N, al_ipos);
  b.CreateStore(llvm::ConstantInt::getFalse(ctx), al_found);

  llvm::BasicBlock* bb_lb_hdr  = llvm::BasicBlock::Create(ctx, "tk_lb_hdr",  fn);
  llvm::BasicBlock* bb_lb_body = llvm::BasicBlock::Create(ctx, "tk_lb_body", fn);
  llvm::BasicBlock* bb_lb_skip = llvm::BasicBlock::Create(ctx, "tk_lb_skip", fn);
  llvm::BasicBlock* bb_lb_done = llvm::BasicBlock::Create(ctx, "tk_lb_done", fn);
  llvm::BasicBlock* bb_lb_exit = llvm::BasicBlock::Create(ctx, "tk_lb_exit", fn);

  (void)bb_pre;
  b.CreateBr(bb_lb_hdr);

  // Header: j PHI, j < N
  b.SetInsertPoint(bb_lb_hdr);
  llvm::PHINode* phi_j = b.CreatePHI(i64, 2, "tk_j");
  phi_j->addIncoming(ci64(0), bb_pre);
  llvm::Value* j_lt_N = b.CreateICmpULT(phi_j, N, "tk_j_lt_N");
  b.CreateCondBr(j_lt_N, bb_lb_body, bb_lb_exit);

  // Body: load score at row j, evaluate comparator.
  b.SetInsertPoint(bb_lb_body);
  llvm::Value* row_w = ci64(static_cast<std::int64_t>(row_lanes));
  llvm::Value* s_off = b.CreateMul(phi_j, row_w, "tk_s_off");
  s_off = b.CreateAdd(s_off,
                      ci64(static_cast<std::int64_t>(score_index)),
                      "tk_s_idx");
  llvm::Value* s_ptr = b.CreateGEP(f64, rows_base, s_off, "tk_s_ptr");
  llvm::Value* es = b.CreateLoad(f64, s_ptr, "tk_es");

  // For descending: !comp <=> es <= new_score (i.e. NOT es > new_score).
  // For ascending : !comp <=> es >= new_score (i.e. NOT es < new_score).
  // FE uses raw operator>/< so any NaN comparison yields false in the
  // comparator (which means !comp is true, so NaN scores end up at the
  // current insertion position). Match that with FCmpOLE / FCmpOGE which
  // also yield false when either operand is NaN — the inverted predicate
  // we need is therefore FCmpOGT/OLT being false. Use FCmpUGT (unordered
  // greater-than) to detect "comp true (strict)" so its negation matches
  // FE: comp is es > new (descending) where NaN -> false; we want stop
  // (insert here) when comp is false. FE: comp = es>vs returns false on
  // NaN (because std comp uses operator>), so lower_bound stops on NaN.
  // FCmpUGT returns true on NaN, so its negation (i.e. ordered-and-not-gt)
  // would skip on NaN — wrong. Use FCmpOGT to mirror operator>: NaN -> false
  // -> comp false -> we stop. Then "insert here" condition is !FCmpOGT.
  llvm::Value* comp_true = nullptr;
  if (descending) {
    comp_true = b.CreateFCmpOGT(es, new_score, "tk_comp_d");
  } else {
    comp_true = b.CreateFCmpOLT(es, new_score, "tk_comp_a");
  }
  llvm::Value* not_comp = b.CreateNot(comp_true, "tk_not_comp");

  llvm::Value* found_now = b.CreateLoad(b.getInt1Ty(), al_found, "tk_f");
  llvm::Value* should_record = b.CreateAnd(
      not_comp, b.CreateNot(found_now, "tk_nf"), "tk_rec");
  b.CreateCondBr(should_record, bb_lb_done, bb_lb_skip);

  // Record: ipos = j, found = true.
  b.SetInsertPoint(bb_lb_done);
  b.CreateStore(phi_j, al_ipos);
  b.CreateStore(llvm::ConstantInt::getTrue(ctx), al_found);
  b.CreateBr(bb_lb_skip);

  // Skip: increment j.
  b.SetInsertPoint(bb_lb_skip);
  llvm::Value* j_next = b.CreateAdd(phi_j, ci64(1), "tk_j_next");
  phi_j->addIncoming(j_next, bb_lb_skip);
  b.CreateBr(bb_lb_hdr);

  b.SetInsertPoint(bb_lb_exit);
  llvm::Value* ipos = b.CreateLoad(i64, al_ipos, "tk_ipos_v");

  // ------------------------------------------------------------------------
  // Step 3: shift rows right starting from min(N, K-1) down to ipos+1.
  //   end_idx = (N < K) ? N : K - 1
  //   for (jj = end_idx; jj > ipos; --jj) row[jj] = row[jj-1]
  // ------------------------------------------------------------------------
  llvm::Value* K_i64 = ci64(static_cast<std::int64_t>(K));
  llvm::Value* Km1   = ci64(static_cast<std::int64_t>(K - 1));
  llvm::Value* n_lt_K = b.CreateICmpULT(N, K_i64, "tk_nltK");
  llvm::Value* end_idx = b.CreateSelect(n_lt_K, N, Km1, "tk_end");

  llvm::BasicBlock* bb_pre_sh = b.GetInsertBlock();
  llvm::BasicBlock* bb_sh_hdr  = llvm::BasicBlock::Create(ctx, "tk_sh_hdr",  fn);
  llvm::BasicBlock* bb_sh_body = llvm::BasicBlock::Create(ctx, "tk_sh_body", fn);
  llvm::BasicBlock* bb_sh_lane = llvm::BasicBlock::Create(ctx, "tk_sh_lane", fn);
  llvm::BasicBlock* bb_sh_lane_step = llvm::BasicBlock::Create(ctx, "tk_sh_lane_step", fn);
  llvm::BasicBlock* bb_sh_step = llvm::BasicBlock::Create(ctx, "tk_sh_step", fn);
  llvm::BasicBlock* bb_sh_exit = llvm::BasicBlock::Create(ctx, "tk_sh_exit", fn);

  b.CreateBr(bb_sh_hdr);

  b.SetInsertPoint(bb_sh_hdr);
  llvm::PHINode* phi_jj = b.CreatePHI(i64, 2, "tk_jj");
  phi_jj->addIncoming(end_idx, bb_pre_sh);
  llvm::Value* jj_gt_ipos = b.CreateICmpUGT(phi_jj, ipos, "tk_jj_gt");
  b.CreateCondBr(jj_gt_ipos, bb_sh_body, bb_sh_exit);

  // Body: copy row[jj-1] -> row[jj], all W lanes.
  b.SetInsertPoint(bb_sh_body);
  llvm::Value* jj_m1 = b.CreateSub(phi_jj, ci64(1), "tk_jjm1");
  b.CreateBr(bb_sh_lane);

  b.SetInsertPoint(bb_sh_lane);
  llvm::PHINode* phi_lane = b.CreatePHI(i64, 2, "tk_lane");
  phi_lane->addIncoming(ci64(0), bb_sh_body);
  llvm::Value* lane_lt_w = b.CreateICmpULT(phi_lane, row_w, "tk_l_lt_w");
  b.CreateCondBr(lane_lt_w, bb_sh_lane_step, bb_sh_step);

  b.SetInsertPoint(bb_sh_lane_step);
  llvm::Value* src_off = b.CreateAdd(b.CreateMul(jj_m1, row_w), phi_lane, "tk_so");
  llvm::Value* dst_off = b.CreateAdd(b.CreateMul(phi_jj, row_w), phi_lane, "tk_do");
  llvm::Value* src_ptr = b.CreateGEP(f64, rows_base, src_off, "tk_sp");
  llvm::Value* dst_ptr = b.CreateGEP(f64, rows_base, dst_off, "tk_dp");
  b.CreateStore(b.CreateLoad(f64, src_ptr, "tk_sv"), dst_ptr);
  llvm::Value* lane_next = b.CreateAdd(phi_lane, ci64(1), "tk_l_next");
  phi_lane->addIncoming(lane_next, bb_sh_lane_step);
  b.CreateBr(bb_sh_lane);

  b.SetInsertPoint(bb_sh_step);
  llvm::Value* jj_next = b.CreateSub(phi_jj, ci64(1), "tk_jj_next");
  phi_jj->addIncoming(jj_next, bb_sh_step);
  b.CreateBr(bb_sh_hdr);

  b.SetInsertPoint(bb_sh_exit);

  // ------------------------------------------------------------------------
  // Step 4: write the new row at slot ipos. Only if ipos < K (which is
  // guaranteed by lower_bound returning at most N <= K, but when N == K we
  // dropped the back via the shift's end_idx clamp; the new slot at ipos is
  // still within [0, K)).
  // ------------------------------------------------------------------------
  // ipos < K guard (defensive — should always hold).
  llvm::Value* ipos_lt_K = b.CreateICmpULT(ipos, K_i64, "tk_ipos_lt_K");
  llvm::BasicBlock* bb_wr_yes = llvm::BasicBlock::Create(ctx, "tk_wr_yes", fn);
  llvm::BasicBlock* bb_wr_skip = llvm::BasicBlock::Create(ctx, "tk_wr_skip", fn);
  b.CreateCondBr(ipos_lt_K, bb_wr_yes, bb_wr_skip);

  b.SetInsertPoint(bb_wr_yes);
  for (std::size_t k = 0; k < row_lanes; ++k) {
    llvm::Value* off_k = b.CreateAdd(
        b.CreateMul(ipos, row_w),
        ci64(static_cast<std::int64_t>(k)),
        "tk_w_off");
    llvm::Value* ptr_k = b.CreateGEP(f64, rows_base, off_k, "tk_w_ptr");
    b.CreateStore(input_lanes[k], ptr_k);
  }
  b.CreateBr(bb_wr_skip);

  b.SetInsertPoint(bb_wr_skip);

  // ------------------------------------------------------------------------
  // Step 5: count_new = min(count + 1, K). Store back as double.
  // ------------------------------------------------------------------------
  llvm::Value* count_p1 = b.CreateAdd(N, ci64(1), "tk_cp1");
  llvm::Value* p1_lt_K  = b.CreateICmpULT(count_p1, K_i64, "tk_p1ltK");
  llvm::Value* count_new = b.CreateSelect(p1_lt_K, count_p1, K_i64, "tk_cn");
  // Re-include the equal case: select picks count_p1 when count_p1 < K, so
  // when count == K we pick K. Good. Convert back to double for storage.
  llvm::Value* count_new_d = b.CreateUIToFP(count_new, f64, "tk_cn_d");
  b.CreateStore(count_new_d, count_ptr);

  // ------------------------------------------------------------------------
  // Step 6: emit count_new records into out_*_arr. record j at slot j.
  //   out_t_arr[j]       = t
  //   out_port_id_arr[j] = out_port_id
  //   out_v_arr[j*num_outputs + k] = state[1 + j*W + k]   for k in [0, W)
  // (num_outputs_per_record == row_lanes for typical TopK -> Output, but we
  //  pass it explicitly so the surrounding emitter can reserve other slots.)
  // ------------------------------------------------------------------------
  llvm::BasicBlock* bb_pre_em = b.GetInsertBlock();
  llvm::BasicBlock* bb_em_hdr  = llvm::BasicBlock::Create(ctx, "tk_em_hdr",  fn);
  llvm::BasicBlock* bb_em_body = llvm::BasicBlock::Create(ctx, "tk_em_body", fn);
  llvm::BasicBlock* bb_em_lane = llvm::BasicBlock::Create(ctx, "tk_em_lane", fn);
  llvm::BasicBlock* bb_em_lane_step = llvm::BasicBlock::Create(ctx, "tk_em_lane_step", fn);
  llvm::BasicBlock* bb_em_step = llvm::BasicBlock::Create(ctx, "tk_em_step", fn);
  llvm::BasicBlock* bb_em_exit = llvm::BasicBlock::Create(ctx, "tk_em_exit", fn);

  b.CreateBr(bb_em_hdr);

  b.SetInsertPoint(bb_em_hdr);
  llvm::PHINode* phi_e = b.CreatePHI(i64, 2, "tk_em_j");
  phi_e->addIncoming(ci64(0), bb_pre_em);
  llvm::Value* e_lt = b.CreateICmpULT(phi_e, count_new, "tk_em_lt");
  b.CreateCondBr(e_lt, bb_em_body, bb_em_exit);

  b.SetInsertPoint(bb_em_body);
  // out_t_arr[j] = t
  llvm::Value* t_slot = b.CreateGEP(i64, out_t_arr, phi_e, "tk_em_t_slot");
  b.CreateStore(t, t_slot);
  // out_port_id_arr[j] = out_port_id
  llvm::Value* p_slot = b.CreateGEP(i32, out_port_id_arr, phi_e, "tk_em_p_slot");
  b.CreateStore(ci32(static_cast<std::int32_t>(out_port_id)), p_slot);
  // Compute base = j * num_outputs_per_record
  llvm::Value* num_out_c = ci64(static_cast<std::int64_t>(num_outputs_per_record));
  llvm::Value* v_base = b.CreateMul(phi_e, num_out_c, "tk_em_vb");
  // Initialize remaining-of-record slots to 0 (defensive when row_lanes <
  // num_outputs_per_record). Skip when they match exactly.
  if (row_lanes < num_outputs_per_record) {
    for (std::size_t kk = row_lanes; kk < num_outputs_per_record; ++kk) {
      llvm::Value* off = b.CreateAdd(v_base,
                                     ci64(static_cast<std::int64_t>(kk)),
                                     "tk_em_pad_off");
      llvm::Value* ptr = b.CreateGEP(f64, out_v_arr, off, "tk_em_pad_ptr");
      b.CreateStore(cf(0.0), ptr);
    }
  }
  b.CreateBr(bb_em_lane);

  b.SetInsertPoint(bb_em_lane);
  llvm::PHINode* phi_em_l = b.CreatePHI(i64, 2, "tk_em_l");
  phi_em_l->addIncoming(ci64(0), bb_em_body);
  llvm::Value* em_l_lt = b.CreateICmpULT(phi_em_l, row_w, "tk_em_l_lt");
  b.CreateCondBr(em_l_lt, bb_em_lane_step, bb_em_step);

  b.SetInsertPoint(bb_em_lane_step);
  // src = state[1 + j*W + lane]
  llvm::Value* src_so = b.CreateAdd(b.CreateMul(phi_e, row_w), phi_em_l, "tk_em_so");
  llvm::Value* src_p  = b.CreateGEP(f64, rows_base, src_so, "tk_em_sp");
  // dst = out_v_arr[v_base + lane]
  llvm::Value* dst_so = b.CreateAdd(v_base, phi_em_l, "tk_em_do");
  llvm::Value* dst_p  = b.CreateGEP(f64, out_v_arr, dst_so, "tk_em_dp");
  b.CreateStore(b.CreateLoad(f64, src_p, "tk_em_sv"), dst_p);
  llvm::Value* em_l_next = b.CreateAdd(phi_em_l, ci64(1), "tk_em_l_n");
  phi_em_l->addIncoming(em_l_next, bb_em_lane_step);
  b.CreateBr(bb_em_lane);

  b.SetInsertPoint(bb_em_step);
  llvm::Value* e_next = b.CreateAdd(phi_e, ci64(1), "tk_em_jn");
  phi_e->addIncoming(e_next, bb_em_step);
  b.CreateBr(bb_em_hdr);

  b.SetInsertPoint(bb_em_exit);

  // Return count_new as i32 to caller.
  llvm::Value* count_new_i32 = b.CreateTrunc(count_new, i32, "tk_cn_i32");
  return count_new_i32;
}

}  // namespace rtbot::jit::emit
