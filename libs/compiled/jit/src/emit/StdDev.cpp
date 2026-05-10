// StdDev.cpp
//
// IR emission for one StdDev<W> step. Mirrors StdDevStage<W> from libs/compiled
// exactly: Kahan compensated ring buffer (same as MA) followed by a variance
// recompute loop and sqrt, yielding the sample standard deviation.
//
// State layout (state_offset + W+3 doubles):
//   [0..W-1]  ring buffer
//   [W]       Kahan sum
//   [W+1]     Kahan compensation
//   [W+2]     count (double)
//
// Variance loop (matches StdDevStage):
//   if count == W: ring_idx = k
//   else:          ring_idx = (pre_idx + 1 + k) % W
// where pre_idx = (count_before_increment) % W.

#include "rtbot/compiled/jit/emit/StdDev.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

StatefulOutput emit_stddev(IrEmissionContext& ec,
                           std::size_t state_offset,
                           std::size_t W,
                           llvm::Value* t, llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();
  auto& mod = ec.mod();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i1  = llvm::Type::getInt1Ty(ctx);

  auto ci = [&](int64_t val) -> llvm::Value* {
    return llvm::ConstantInt::get(i64, val);
  };
  auto cf = [&](double val) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, val);
  };

  const std::size_t IDX_SUM   = state_offset + W;
  const std::size_t IDX_COMP  = state_offset + W + 1;
  const std::size_t IDX_COUNT = state_offset + W + 2;

  auto sum_ptr   = ec.state_gep(IDX_SUM);
  auto comp_ptr  = ec.state_gep(IDX_COMP);
  auto count_ptr = ec.state_gep(IDX_COUNT);
  auto ring_base = ec.state_gep(state_offset);

  llvm::Value* w_d   = cf(static_cast<double>(W));
  llvm::Value* w_u64 = ci(static_cast<int64_t>(W));
  llvm::Value* one_d = cf(1.0);

  // Load pre-increment count.
  llvm::Value* count_d   = b.CreateLoad(f64, count_ptr, "sd_count_d");
  llvm::Value* count_u64 = b.CreateFPToUI(count_d, i64, "sd_count_u64");
  llvm::Value* idx_u64   = b.CreateURem(count_u64, w_u64, "sd_idx");

  llvm::Value* cond_sub = b.CreateFCmpOGE(count_d, w_d, "sd_cond_sub");

  llvm::Function* fn = b.GetInsertBlock()->getParent();

  llvm::BasicBlock* bb_sub      = llvm::BasicBlock::Create(ctx, "sd_kahan_sub",  fn);
  llvm::BasicBlock* bb_add      = llvm::BasicBlock::Create(ctx, "sd_kahan_add",  fn);
  llvm::BasicBlock* bb_check    = llvm::BasicBlock::Create(ctx, "sd_emit_check", fn);
  llvm::BasicBlock* bb_var_pre  = llvm::BasicBlock::Create(ctx, "sd_var_pre",    fn);
  llvm::BasicBlock* bb_var_hdr  = llvm::BasicBlock::Create(ctx, "sd_var_hdr",    fn);
  llvm::BasicBlock* bb_var_body = llvm::BasicBlock::Create(ctx, "sd_var_body",   fn);
  llvm::BasicBlock* bb_var_done = llvm::BasicBlock::Create(ctx, "sd_var_done",   fn);
  llvm::BasicBlock* bb_false    = llvm::BasicBlock::Create(ctx, "sd_emit_false", fn);
  llvm::BasicBlock* bb_merge    = llvm::BasicBlock::Create(ctx, "sd_merge",      fn);

  b.CreateCondBr(cond_sub, bb_sub, bb_add);

  // --- kahan_sub -----------------------------------------------------------
  b.SetInsertPoint(bb_sub);
  {
    llvm::Value* ring_ptr = b.CreateGEP(f64, ring_base, idx_u64, "sd_ring_sub");
    llvm::Value* leaving  = b.CreateLoad(f64, ring_ptr, "sd_leaving");
    ec.emit_kahan_subtract(sum_ptr, comp_ptr, leaving);
  }
  b.CreateBr(bb_add);

  // --- kahan_add -----------------------------------------------------------
  b.SetInsertPoint(bb_add);
  {
    llvm::Value* ring_ptr = b.CreateGEP(f64, ring_base, idx_u64, "sd_ring_add");
    b.CreateStore(v, ring_ptr);
    ec.emit_kahan_add(sum_ptr, comp_ptr, v);
  }
  // post-increment count
  llvm::Value* count_new = b.CreateFAdd(count_d, one_d, "sd_count_new");
  b.CreateStore(count_new, count_ptr);
  b.CreateBr(bb_check);

  // --- emit_check: count_new < W → warmup ----------------------------------
  b.SetInsertPoint(bb_check);
  llvm::Value* cond_warm = b.CreateFCmpOLT(count_new, w_d, "sd_cond_warm");
  b.CreateCondBr(cond_warm, bb_false, bb_var_pre);

  // --- var_pre: loop-invariant setup (no PHIs here) ------------------------
  // Variance loop mirrors StdDevStage:
  //   pre_idx = pre_count % W  (where pre_count = count before increment = count_d)
  //   cond_eq_w: (post_count as u64) == W
  //   if cond_eq_w: ring_idx = k
  //   else:         ring_idx = (pre_idx + 1 + k) % W
  b.SetInsertPoint(bb_var_pre);
  llvm::Value* post_cnt_u64 = b.CreateFPToUI(count_new, i64, "sd_post_cnt_u64");
  llvm::Value* cond_eq_w    = b.CreateICmpEQ(post_cnt_u64, w_u64, "sd_eq_w");
  llvm::Value* pre_idx      = idx_u64;   // pre_count % W computed above
  llvm::Value* sum_f        = b.CreateLoad(f64, sum_ptr, "sd_sum_f");
  llvm::Value* mean         = b.CreateFDiv(sum_f, w_d, "sd_mean");
  b.CreateBr(bb_var_hdr);

  // --- var_hdr: PHI loop header --------------------------------------------
  b.SetInsertPoint(bb_var_hdr);
  llvm::PHINode* phi_k  = b.CreatePHI(i64, 2, "sd_k");
  llvm::PHINode* phi_m2 = b.CreatePHI(f64, 2, "sd_m2");
  phi_k->addIncoming(ci(0),   bb_var_pre);
  phi_m2->addIncoming(cf(0.0), bb_var_pre);
  b.CreateCondBr(b.CreateICmpULT(phi_k, w_u64, "sd_k_lt_w"),
                 bb_var_body, bb_var_done);

  // --- var_body: (ring[ring_idx] - mean)^2 ---------------------------------
  b.SetInsertPoint(bb_var_body);
  {
    // wrapped = (pre_idx + 1 + k) % W
    llvm::Value* wp1     = b.CreateAdd(pre_idx, ci(1), "sd_wp1");
    llvm::Value* wpk     = b.CreateAdd(wp1, phi_k, "sd_wpk");
    llvm::Value* wrapped = b.CreateURem(wpk, w_u64, "sd_wrapped");
    llvm::Value* ring_idx = b.CreateSelect(cond_eq_w, phi_k, wrapped, "sd_ring_idx");

    llvm::Value* sample = b.CreateLoad(f64, b.CreateGEP(f64, ring_base, ring_idx, "sd_s_ptr"), "sd_sample");
    llvm::Value* d      = b.CreateFSub(sample, mean, "sd_d");
    llvm::Value* d2     = b.CreateFMul(d, d, "sd_d2");
    llvm::Value* m2n    = b.CreateFAdd(phi_m2, d2, "sd_m2n");
    llvm::Value* k1     = b.CreateAdd(phi_k, ci(1), "sd_k1");
    phi_k->addIncoming(k1,  bb_var_body);
    phi_m2->addIncoming(m2n, bb_var_body);
  }
  b.CreateBr(bb_var_hdr);

  // --- var_done: sqrt(m2 / (W-1)) ------------------------------------------
  b.SetInsertPoint(bb_var_done);
  llvm::Value* variance = b.CreateFDiv(phi_m2, cf(static_cast<double>(W - 1)), "sd_var");
  llvm::Function* sqrt_fn = llvm::Intrinsic::getDeclaration(&mod, llvm::Intrinsic::sqrt, {f64});
  llvm::Value* out_v_true = b.CreateCall(sqrt_fn, {variance}, "sd_out_v");
  b.CreateBr(bb_merge);

  // --- emit_false ----------------------------------------------------------
  b.SetInsertPoint(bb_false);
  b.CreateBr(bb_merge);

  // --- merge: PHI for (out_v, emit_flag) -----------------------------------
  b.SetInsertPoint(bb_merge);
  llvm::PHINode* phi_v = b.CreatePHI(f64, 2, "sd_phi_v");
  phi_v->addIncoming(out_v_true, bb_var_done);
  phi_v->addIncoming(cf(0.0),    bb_false);

  llvm::PHINode* phi_flag = b.CreatePHI(i1, 2, "sd_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_var_done);
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_false);

  return StatefulOutput{t, phi_v, phi_flag};
}

SDUpdateResult emit_stddev_update(IrEmissionContext& ec,
                                  std::size_t state_offset,
                                  std::size_t W,
                                  llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  auto ci = [&](int64_t val) -> llvm::Value* { return llvm::ConstantInt::get(i64, val); };
  auto cf = [&](double val)  -> llvm::Value* { return llvm::ConstantFP::get(f64, val); };

  const std::size_t IDX_SUM   = state_offset + W;
  const std::size_t IDX_COMP  = state_offset + W + 1;
  const std::size_t IDX_COUNT = state_offset + W + 2;

  auto sum_ptr   = ec.state_gep(IDX_SUM);
  auto comp_ptr  = ec.state_gep(IDX_COMP);
  auto count_ptr = ec.state_gep(IDX_COUNT);
  auto ring_base = ec.state_gep(state_offset);

  llvm::Value* w_d   = cf(static_cast<double>(W));
  llvm::Value* w_u64 = ci(static_cast<int64_t>(W));
  llvm::Value* one_d = cf(1.0);

  llvm::Function* fn = b.GetInsertBlock()->getParent();
  llvm::BasicBlock* bb_sub = llvm::BasicBlock::Create(ctx, "sdu_kahan_sub", fn);
  llvm::BasicBlock* bb_add = llvm::BasicBlock::Create(ctx, "sdu_kahan_add", fn);

  // Load pre-increment count.
  llvm::Value* count_d   = b.CreateLoad(f64, count_ptr, "sdu_count");
  llvm::Value* count_u64 = b.CreateFPToUI(count_d, i64, "sdu_count_u64");
  llvm::Value* idx_u64   = b.CreateURem(count_u64, w_u64, "sdu_idx");

  llvm::Value* cond_sub = b.CreateFCmpOGE(count_d, w_d, "sdu_cond_sub");
  b.CreateCondBr(cond_sub, bb_sub, bb_add);

  // --- kahan_sub: remove leaving element ------------------------------------
  b.SetInsertPoint(bb_sub);
  {
    llvm::Value* ring_ptr = b.CreateGEP(f64, ring_base, idx_u64, "sdu_ring_sub");
    llvm::Value* leaving  = b.CreateLoad(f64, ring_ptr, "sdu_leaving");
    ec.emit_kahan_subtract(sum_ptr, comp_ptr, leaving);
  }
  b.CreateBr(bb_add);

  // --- kahan_add: store v in ring, Kahan add --------------------------------
  b.SetInsertPoint(bb_add);
  {
    llvm::Value* ring_ptr = b.CreateGEP(f64, ring_base, idx_u64, "sdu_ring_add");
    b.CreateStore(v, ring_ptr);
    ec.emit_kahan_add(sum_ptr, comp_ptr, v);
  }
  llvm::Value* count_new = b.CreateFAdd(count_d, one_d, "sdu_count_new");
  b.CreateStore(count_new, count_ptr);

  // emit_flag: count_new >= W — direct comparison, no PHI node.
  llvm::Value* emit_flag = b.CreateFCmpOGE(count_new, w_d, "sdu_emit_flag");

  return SDUpdateResult{emit_flag, ring_base, sum_ptr, count_new, idx_u64, w_d, w_u64};
}

llvm::Value* emit_stddev_output(IrEmissionContext& ec, const SDUpdateResult& upd,
                                std::size_t W) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();
  auto& mod = ec.mod();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  auto ci = [&](int64_t val) -> llvm::Value* { return llvm::ConstantInt::get(i64, val); };
  auto cf = [&](double val)  -> llvm::Value* { return llvm::ConstantFP::get(f64, val); };

  llvm::Function* fn = b.GetInsertBlock()->getParent();
  llvm::BasicBlock* bb_entry = b.GetInsertBlock();  // predecessor for PHI init

  // post_count as u64 for the cond_eq_w check.
  llvm::Value* post_cnt_u64 = b.CreateFPToUI(upd.count_new, i64, "sdo_post_cnt_u64");
  llvm::Value* cond_eq_w    = b.CreateICmpEQ(post_cnt_u64, upd.w_u64, "sdo_eq_w");

  // Load sum, compute mean.
  llvm::Value* sum_f = b.CreateLoad(f64, upd.sum_ptr, "sdo_sum");
  llvm::Value* mean  = b.CreateFDiv(sum_f, upd.w_d, "sdo_mean");

  // Variance loop: same structure as StdDevStage and spike.
  llvm::BasicBlock* bb_var_hdr  = llvm::BasicBlock::Create(ctx, "sdo_var_hdr",  fn);
  llvm::BasicBlock* bb_var_body = llvm::BasicBlock::Create(ctx, "sdo_var_body", fn);
  llvm::BasicBlock* bb_var_done = llvm::BasicBlock::Create(ctx, "sdo_var_done", fn);

  b.CreateBr(bb_var_hdr);

  b.SetInsertPoint(bb_var_hdr);
  llvm::PHINode* phi_k  = b.CreatePHI(i64, 2, "sdo_k");
  llvm::PHINode* phi_m2 = b.CreatePHI(f64, 2, "sdo_m2");
  phi_k->addIncoming(ci(0),    bb_entry);
  phi_m2->addIncoming(cf(0.0), bb_entry);
  b.CreateCondBr(b.CreateICmpULT(phi_k, upd.w_u64, "sdo_k_lt_w"), bb_var_body, bb_var_done);

  b.SetInsertPoint(bb_var_body);
  {
    llvm::Value* wp1     = b.CreateAdd(upd.pre_idx, ci(1), "sdo_wp1");
    llvm::Value* wpk     = b.CreateAdd(wp1, phi_k, "sdo_wpk");
    llvm::Value* wrapped = b.CreateURem(wpk, upd.w_u64, "sdo_wrapped");
    llvm::Value* ring_idx = b.CreateSelect(cond_eq_w, phi_k, wrapped, "sdo_ring_idx");
    llvm::Value* sample  = b.CreateLoad(f64, b.CreateGEP(f64, upd.ring_base, ring_idx, "sdo_s_ptr"), "sdo_sample");
    llvm::Value* d       = b.CreateFSub(sample, mean, "sdo_d");
    llvm::Value* d2      = b.CreateFMul(d, d, "sdo_d2");
    llvm::Value* m2n     = b.CreateFAdd(phi_m2, d2, "sdo_m2n");
    llvm::Value* k1      = b.CreateAdd(phi_k, ci(1), "sdo_k1");
    phi_k->addIncoming(k1,  bb_var_body);
    phi_m2->addIncoming(m2n, bb_var_body);
  }
  b.CreateBr(bb_var_hdr);

  b.SetInsertPoint(bb_var_done);
  llvm::Value* variance = b.CreateFDiv(phi_m2, cf(static_cast<double>(W - 1)), "sdo_var");
  llvm::Function* sqrt_fn = llvm::Intrinsic::getDeclaration(&mod, llvm::Intrinsic::sqrt, {f64});
  return b.CreateCall(sqrt_fn, {variance}, "sdo_out");
}

}  // namespace rtbot::jit::emit
