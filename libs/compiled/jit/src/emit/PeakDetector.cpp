// PeakDetector.cpp
//
// IR emission for one PeakDetector<W> step. Mirrors PeakDetectorStage<W>
// from libs/compiled/include/rtbot/compiled/PeakDetectorStage.h exactly.
//
// State layout (state_offset + 2*W + 1 doubles):
//   [0..W-1]   ring_v  — sliding window of values
//   [W..2W-1]  ring_t  — sliding window of timestamps (bit-cast i64 <-> double)
//   [2W]       count   — sample count (stored as double)
//
// Algorithm (unified-expression form):
//   idx         = count % W
//   ring_v[idx] = v;  ring_t[idx] = bitcast(t, double)
//   count++
//   if count < W: return (false, _, _)          // warmup
//   center_step = W / 2
//   center_idx  = (idx + 1 + center_step) % W   // same for count==W and count>W
//   center_v    = ring_v[center_idx]
//   for k in [0, W):
//     i = (idx + 1 + k) % W
//     if i != center_idx && ring_v[i] >= center_v: is_peak = false; break
//   if is_peak:
//     return (true, bitcast(ring_t[center_idx], i64), center_v)
//   else:
//     return (false, _, _)

#include "rtbot/compiled/jit/emit/PeakDetector.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

StatefulOutput emit_peak_detector(IrEmissionContext& ec,
                                   std::size_t state_offset,
                                   std::size_t W,
                                   llvm::Value* t, llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i1  = llvm::Type::getInt1Ty(ctx);

  auto ci = [&](int64_t val) -> llvm::Value* {
    return llvm::ConstantInt::get(i64, val);
  };
  auto cf = [&](double val) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, val);
  };

  // Absolute state-slot indices.
  const std::size_t IDX_RING_V = state_offset;
  const std::size_t IDX_RING_T = state_offset + W;
  const std::size_t IDX_COUNT  = state_offset + 2 * W;

  llvm::Value* ring_v_base = ec.state_gep(IDX_RING_V);
  llvm::Value* ring_t_base = ec.state_gep(IDX_RING_T);
  llvm::Value* count_ptr   = ec.state_gep(IDX_COUNT);

  llvm::Value* w_u64 = ci(static_cast<int64_t>(W));
  llvm::Value* w_d   = cf(static_cast<double>(W));
  llvm::Value* one_d = cf(1.0);

  // Load pre-increment count and compute ring write index.
  llvm::Value* count_d   = b.CreateLoad(f64, count_ptr, "pd_count_d");
  llvm::Value* count_u64 = b.CreateFPToUI(count_d, i64, "pd_count_u64");
  llvm::Value* idx_u64   = b.CreateURem(count_u64, w_u64, "pd_idx");

  // ring_v[idx] = v
  b.CreateStore(v, b.CreateGEP(f64, ring_v_base, idx_u64, "pd_rv_ptr"));

  // ring_t[idx] = bitcast(t, double)
  b.CreateStore(b.CreateBitCast(t, f64, "pd_t_bc"),
                b.CreateGEP(f64, ring_t_base, idx_u64, "pd_rt_ptr"));

  // ++count
  llvm::Value* count_new = b.CreateFAdd(count_d, one_d, "pd_count_new");
  b.CreateStore(count_new, count_ptr);

  llvm::Function* fn = b.GetInsertBlock()->getParent();

  llvm::BasicBlock* bb_warmup    = llvm::BasicBlock::Create(ctx, "pd_warmup",    fn);
  llvm::BasicBlock* bb_scan_pre  = llvm::BasicBlock::Create(ctx, "pd_scan_pre",  fn);
  llvm::BasicBlock* bb_scan_hdr  = llvm::BasicBlock::Create(ctx, "pd_scan_hdr",  fn);
  llvm::BasicBlock* bb_scan_body = llvm::BasicBlock::Create(ctx, "pd_scan_body", fn);
  llvm::BasicBlock* bb_scan_cmp  = llvm::BasicBlock::Create(ctx, "pd_scan_cmp",  fn);
  llvm::BasicBlock* bb_scan_next = llvm::BasicBlock::Create(ctx, "pd_scan_next", fn);
  llvm::BasicBlock* bb_scan_done = llvm::BasicBlock::Create(ctx, "pd_scan_done", fn);
  llvm::BasicBlock* bb_merge     = llvm::BasicBlock::Create(ctx, "pd_merge",     fn);

  // Branch on count_new < W → warmup.
  llvm::Value* cond_warm = b.CreateFCmpOLT(count_new, w_d, "pd_cond_warm");
  b.CreateCondBr(cond_warm, bb_warmup, bb_scan_pre);

  // --- warmup: no output ---------------------------------------------------
  b.SetInsertPoint(bb_warmup);
  b.CreateBr(bb_merge);

  // --- scan_pre: compute center_idx and center_v (loop-invariant) ----------
  b.SetInsertPoint(bb_scan_pre);
  // Unified center_idx = (idx + 1 + center_step) % W
  const int64_t center_step  = static_cast<int64_t>(W / 2);
  llvm::Value* ci_p1         = b.CreateAdd(idx_u64, ci(1),           "pd_ci_p1");
  llvm::Value* ci_pcs        = b.CreateAdd(ci_p1,   ci(center_step), "pd_ci_pcs");
  llvm::Value* center_idx    = b.CreateURem(ci_pcs, w_u64,           "pd_center_idx");
  llvm::Value* cv_ptr        = b.CreateGEP(f64, ring_v_base, center_idx, "pd_cv_ptr");
  llvm::Value* center_v      = b.CreateLoad(f64, cv_ptr, "pd_center_v");
  b.CreateBr(bb_scan_hdr);

  // --- scan_hdr: PHI for k and is_peak ------------------------------------
  b.SetInsertPoint(bb_scan_hdr);
  llvm::PHINode* phi_k       = b.CreatePHI(i64, 2, "pd_k");
  llvm::PHINode* phi_is_peak = b.CreatePHI(i1,  2, "pd_is_peak");
  phi_k->addIncoming(ci(0),                               bb_scan_pre);
  phi_is_peak->addIncoming(llvm::ConstantInt::getTrue(ctx), bb_scan_pre);

  llvm::Value* k_lt_w = b.CreateICmpULT(phi_k, w_u64, "pd_k_lt_w");
  b.CreateCondBr(k_lt_w, bb_scan_body, bb_scan_done);

  // --- scan_body: compute i = (idx + 1 + k) % W; check if i != center_idx -
  b.SetInsertPoint(bb_scan_body);
  llvm::Value* bi_p1       = b.CreateAdd(idx_u64, ci(1),   "pd_bi_p1");
  llvm::Value* bi_pk       = b.CreateAdd(bi_p1,   phi_k,   "pd_bi_pk");
  llvm::Value* i_idx       = b.CreateURem(bi_pk,  w_u64,   "pd_i_idx");
  llvm::Value* not_center  = b.CreateICmpNE(i_idx, center_idx, "pd_not_center");
  b.CreateCondBr(not_center, bb_scan_cmp, bb_scan_next);

  // --- scan_cmp: load ring_v[i] and test >= center_v ----------------------
  b.SetInsertPoint(bb_scan_cmp);
  llvm::Value* ri_ptr   = b.CreateGEP(f64, ring_v_base, i_idx, "pd_ri_ptr");
  llvm::Value* ring_vi  = b.CreateLoad(f64, ri_ptr, "pd_ring_vi");
  llvm::Value* ge_cv    = b.CreateFCmpOGE(ring_vi, center_v, "pd_ge_cv");
  // If ring_vi >= center_v: this slot beats center → not a peak.
  // is_peak_after_cmp = phi_is_peak AND NOT ge_cv
  llvm::Value* not_ge         = b.CreateNot(ge_cv, "pd_not_ge");
  llvm::Value* is_peak_after  = b.CreateAnd(phi_is_peak, not_ge, "pd_is_peak_after");
  b.CreateBr(bb_scan_next);

  // --- scan_next: thread is_peak through, advance k -----------------------
  b.SetInsertPoint(bb_scan_next);
  llvm::PHINode* phi_ip_next = b.CreatePHI(i1, 2, "pd_ip_next");
  phi_ip_next->addIncoming(is_peak_after, bb_scan_cmp);
  phi_ip_next->addIncoming(phi_is_peak,   bb_scan_body);

  llvm::Value* k1 = b.CreateAdd(phi_k, ci(1), "pd_k1");
  phi_k->addIncoming(k1,           bb_scan_next);
  phi_is_peak->addIncoming(phi_ip_next, bb_scan_next);
  b.CreateBr(bb_scan_hdr);

  // --- scan_done: read center outputs ------------------------------------
  b.SetInsertPoint(bb_scan_done);
  llvm::Value* ct_ptr  = b.CreateGEP(f64, ring_t_base, center_idx, "pd_ct_ptr");
  llvm::Value* ct_d    = b.CreateLoad(f64, ct_ptr, "pd_ct_d");
  llvm::Value* out_t_v = b.CreateBitCast(ct_d, i64, "pd_out_t");
  b.CreateBr(bb_merge);

  // --- merge: PHI for emit_flag, out_t, out_v ----------------------------
  b.SetInsertPoint(bb_merge);

  llvm::PHINode* phi_flag  = b.CreatePHI(i1,  2, "pd_phi_flag");
  phi_flag->addIncoming(phi_is_peak,                      bb_scan_done);
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_warmup);

  llvm::PHINode* phi_out_t = b.CreatePHI(i64, 2, "pd_phi_out_t");
  phi_out_t->addIncoming(out_t_v, bb_scan_done);
  phi_out_t->addIncoming(ci(0),   bb_warmup);

  llvm::PHINode* phi_out_v = b.CreatePHI(f64, 2, "pd_phi_out_v");
  phi_out_v->addIncoming(center_v, bb_scan_done);
  phi_out_v->addIncoming(cf(0.0),  bb_warmup);

  return StatefulOutput{phi_out_t, phi_out_v, phi_flag};
}

}  // namespace rtbot::jit::emit
