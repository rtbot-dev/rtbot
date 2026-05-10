// IIR.cpp
//
// IR emission for one IIR (Infinite Impulse Response) filter step.
// Mirrors FE IIR_UPDATE (FusedScalarEval.h case 43).
//
// State layout (state_offset + 4 + B_len + A_len doubles):
//   [0]              x_head  — next x_ring write position (double, read as size_t)
//   [1]              x_count — number of x samples accumulated so far
//   [2]              y_head  — next y_ring write position (double, read as size_t)
//   [3]              y_count — number of y samples accumulated so far
//   [4..4+B-1]       x_ring  — input history ring buffer
//   [4+B..4+B+A-1]   y_ring  — output history ring buffer
//
// Coefficients are baked into the IR as a private global constant array.
// Layout: b_coeffs[0..B-1] then a_coeffs[0..A-1].
//
// Two Kahan-compensated loops:
//   b-loop: k=0..B_len-1
//     ri = (xi + B_len - k) % B_len
//     term = b[k] * x_ring[ri] - comp
//     t = y_n + term; comp = (t - y_n) - term; y_n = t
//   a-loop: k=0..y_use-1  (y_use = min(y_count, A_len), runtime quantity)
//     yi_back = (y_head + A_len - 1 - k) % A_len
//     term = -(a[k] * y_ring[yi_back]) - comp
//     t = y_n + term; comp = (t - y_n) - term; y_n = t
//
// Emits once x_ring is full (x_count_new >= B_len).
// Both loops follow the FIR "PHI-in-body, latch-in-next" pattern.

#include "rtbot/compiled/jit/emit/IIR.h"

#include <string>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

StatefulOutput emit_iir(IrEmissionContext& ec,
                        std::size_t state_offset,
                        std::size_t B_len,
                        std::size_t A_len,
                        const std::vector<double>& coefficients,
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

  // --- Bake coefficients into a private global constant array -----------------
  const std::size_t total_coeffs = B_len + A_len;
  auto* coef_ty = llvm::ArrayType::get(f64, total_coeffs);
  std::vector<llvm::Constant*> coef_consts;
  coef_consts.reserve(total_coeffs);
  for (double c : coefficients)
    coef_consts.push_back(llvm::ConstantFP::get(f64, c));
  auto* coef_init   = llvm::ConstantArray::get(coef_ty, coef_consts);
  std::string gname = "iir_coeffs_" + std::to_string(state_offset);
  auto* coef_global = new llvm::GlobalVariable(
      ec.mod(), coef_ty, /*isConstant=*/true,
      llvm::GlobalValue::PrivateLinkage, coef_init, gname);

  // --- State slot pointers ---------------------------------------------------
  const std::size_t IDX_X_HEAD  = state_offset + 0;
  const std::size_t IDX_X_COUNT = state_offset + 1;
  const std::size_t IDX_Y_HEAD  = state_offset + 2;
  const std::size_t IDX_Y_COUNT = state_offset + 3;
  const std::size_t IDX_X_RING  = state_offset + 4;
  const std::size_t IDX_Y_RING  = state_offset + 4 + B_len;

  llvm::Value* x_head_ptr  = ec.state_gep(IDX_X_HEAD);
  llvm::Value* x_count_ptr = ec.state_gep(IDX_X_COUNT);
  llvm::Value* y_head_ptr  = ec.state_gep(IDX_Y_HEAD);
  llvm::Value* y_count_ptr = ec.state_gep(IDX_Y_COUNT);
  llvm::Value* x_ring_base = ec.state_gep(IDX_X_RING);
  llvm::Value* y_ring_base = (A_len > 0) ? ec.state_gep(IDX_Y_RING) : nullptr;

  llvm::Value* b_len_d   = cf(static_cast<double>(B_len));
  llvm::Value* b_len_u64 = ci(static_cast<int64_t>(B_len));
  llvm::Value* a_len_u64 = ci(static_cast<int64_t>(A_len));
  llvm::Value* one_d     = cf(1.0);

  // --- 1. Load x_head, write v into x_ring[xi], advance head, bump count ----
  llvm::Value* x_head_d  = b.CreateLoad(f64, x_head_ptr,  "iir_xhd");
  llvm::Value* xi        = b.CreateFPToUI(x_head_d, i64,  "iir_xi");
  llvm::Value* x_count_d = b.CreateLoad(f64, x_count_ptr, "iir_xcnt");

  llvm::Value* xring_w = b.CreateGEP(f64, x_ring_base, xi, "iir_xring_w");
  b.CreateStore(v, xring_w);

  llvm::Value* xi_p1        = b.CreateAdd(xi, ci(1), "iir_xi_p1");
  llvm::Value* xhd_new_u    = b.CreateURem(xi_p1, b_len_u64, "iir_xhd_new_u");
  llvm::Value* xhd_new_d    = b.CreateUIToFP(xhd_new_u, f64, "iir_xhd_new_d");
  b.CreateStore(xhd_new_d, x_head_ptr);

  // if (x_count < B_len) x_count = x_count + 1.0
  llvm::Value* cond_x_nfull = b.CreateFCmpOLT(x_count_d, b_len_d, "iir_x_nfull");
  llvm::Value* xcnt_inc     = b.CreateFAdd(x_count_d, one_d, "iir_xcnt_inc");
  llvm::Value* x_count_new  = b.CreateSelect(cond_x_nfull, xcnt_inc, x_count_d,
                                              "iir_xcnt_new");
  b.CreateStore(x_count_new, x_count_ptr);

  // --- 2. Warmup gate ---------------------------------------------------------
  llvm::Function* fn = b.GetInsertBlock()->getParent();

  llvm::BasicBlock* bb_warmup  = llvm::BasicBlock::Create(ctx, "iir_warmup",  fn);
  // b-loop: unconditional entry block (mirrors FIR's bb_dot) + PHI body block
  llvm::BasicBlock* bb_b_dot   = llvm::BasicBlock::Create(ctx, "iir_b_dot",   fn);
  llvm::BasicBlock* bb_b_body  = llvm::BasicBlock::Create(ctx, "iir_b_body",  fn);
  llvm::BasicBlock* bb_b_next  = llvm::BasicBlock::Create(ctx, "iir_b_next",  fn);
  llvm::BasicBlock* bb_b_exit  = llvm::BasicBlock::Create(ctx, "iir_b_exit",  fn);
  // a-loop
  llvm::BasicBlock* bb_a_pre   = llvm::BasicBlock::Create(ctx, "iir_a_pre",   fn);
  llvm::BasicBlock* bb_a_dot   = llvm::BasicBlock::Create(ctx, "iir_a_dot",   fn);
  llvm::BasicBlock* bb_a_body  = llvm::BasicBlock::Create(ctx, "iir_a_body",  fn);
  llvm::BasicBlock* bb_a_next  = llvm::BasicBlock::Create(ctx, "iir_a_next",  fn);
  llvm::BasicBlock* bb_a_exit  = llvm::BasicBlock::Create(ctx, "iir_a_exit",  fn);
  // y_ring update
  llvm::BasicBlock* bb_y_upd   = llvm::BasicBlock::Create(ctx, "iir_y_upd",   fn);
  llvm::BasicBlock* bb_y_skip  = llvm::BasicBlock::Create(ctx, "iir_y_skip",  fn);
  llvm::BasicBlock* bb_merge   = llvm::BasicBlock::Create(ctx, "iir_merge",   fn);

  llvm::Value* cond_warm = b.CreateFCmpOLT(x_count_new, b_len_d, "iir_cond_warm");
  b.CreateCondBr(cond_warm, bb_warmup, bb_b_dot);

  // --- warmup: no output -----------------------------------------------------
  b.SetInsertPoint(bb_warmup);
  b.CreateBr(bb_merge);

  // --- 3. b-loop: k = 0 .. B_len-1 -------------------------------------------
  // Pattern: bb_b_dot → bb_b_body (PHI here) → bb_b_next (latch) → bb_b_body or bb_b_exit

  b.SetInsertPoint(bb_b_dot);
  b.CreateBr(bb_b_body);

  b.SetInsertPoint(bb_b_body);
  llvm::PHINode* phi_bk      = b.CreatePHI(i64, 2, "iir_bk");
  llvm::PHINode* phi_byn     = b.CreatePHI(f64, 2, "iir_byn");
  llvm::PHINode* phi_bcomp   = b.CreatePHI(f64, 2, "iir_bcomp");
  phi_bk->addIncoming(ci(0),     bb_b_dot);
  phi_byn->addIncoming(cf(0.0),  bb_b_dot);
  phi_bcomp->addIncoming(cf(0.0), bb_b_dot);

  // ri = (xi + B_len - k) % B_len
  llvm::Value* b_base  = b.CreateAdd(xi, b_len_u64, "iir_b_base");
  llvm::Value* b_sub   = b.CreateSub(b_base, phi_bk, "iir_b_sub");
  llvm::Value* b_ri    = b.CreateURem(b_sub, b_len_u64, "iir_b_ri");

  // b_coef = coef_global[0][k]
  llvm::Value* b_cptr  = b.CreateGEP(coef_ty, coef_global, {ci(0), phi_bk}, "iir_b_cptr");
  llvm::Value* b_coef  = b.CreateLoad(f64, b_cptr, "iir_b_coef");

  // x_val = x_ring[ri]
  llvm::Value* b_xptr  = b.CreateGEP(f64, x_ring_base, b_ri, "iir_b_xptr");
  llvm::Value* b_xval  = b.CreateLoad(f64, b_xptr, "iir_b_xval");

  // Kahan step: term = b_coef * x_val - comp
  llvm::Value* b_prod  = b.CreateFMul(b_coef, b_xval, "iir_b_prod");
  llvm::Value* b_term  = b.CreateFSub(b_prod, phi_bcomp, "iir_b_term");
  llvm::Value* b_tnew  = b.CreateFAdd(phi_byn, b_term, "iir_b_tnew");
  llvm::Value* b_cdiff = b.CreateFSub(b_tnew, phi_byn, "iir_b_cdiff");
  llvm::Value* b_cnew  = b.CreateFSub(b_cdiff, b_term, "iir_b_cnew");
  b.CreateBr(bb_b_next);

  b.SetInsertPoint(bb_b_next);
  llvm::Value* bk1     = b.CreateAdd(phi_bk, ci(1), "iir_bk1");
  llvm::Value* bk_done = b.CreateICmpULT(bk1, b_len_u64, "iir_bk_done");
  phi_bk->addIncoming(bk1,    bb_b_next);
  phi_byn->addIncoming(b_tnew, bb_b_next);
  phi_bcomp->addIncoming(b_cnew, bb_b_next);
  b.CreateCondBr(bk_done, bb_b_body, bb_b_exit);

  b.SetInsertPoint(bb_b_exit);
  // b_tnew / b_cnew are the final y_n / y_comp after the b-loop.
  // These are defined in bb_b_body (last iteration), dominated by bb_b_exit.
  b.CreateBr(bb_a_pre);

  // --- 4. a-loop pre: compute y_use = min(y_count, A_len) --------------------
  b.SetInsertPoint(bb_a_pre);
  llvm::Value* y_count_d   = b.CreateLoad(f64, y_count_ptr, "iir_ycnt");
  llvm::Value* y_count_u64 = b.CreateFPToUI(y_count_d, i64, "iir_ycnt_u64");
  llvm::Value* yuse_cond   = b.CreateICmpULT(y_count_u64, a_len_u64, "iir_yuse_cond");
  llvm::Value* y_use       = b.CreateSelect(yuse_cond, y_count_u64, a_len_u64,
                                             "iir_y_use");
  llvm::Value* y_head_d    = b.CreateLoad(f64, y_head_ptr, "iir_yhd");
  llvm::Value* y_head_u64  = b.CreateFPToUI(y_head_d, i64, "iir_yhd_u64");
  b.CreateBr(bb_a_dot);

  // --- 5. a-loop: k = 0 .. y_use-1 -------------------------------------------
  // Same PHI-in-body pattern as b-loop.

  b.SetInsertPoint(bb_a_dot);
  // Check if y_use == 0 → skip loop entirely
  llvm::Value* yuse_zero = b.CreateICmpEQ(y_use, ci(0), "iir_yuse_zero");
  b.CreateCondBr(yuse_zero, bb_a_exit, bb_a_body);

  b.SetInsertPoint(bb_a_body);
  llvm::PHINode* phi_ak    = b.CreatePHI(i64, 2, "iir_ak");
  llvm::PHINode* phi_ayn   = b.CreatePHI(f64, 2, "iir_ayn");
  llvm::PHINode* phi_acomp = b.CreatePHI(f64, 2, "iir_acomp");
  phi_ak->addIncoming(ci(0),    bb_a_dot);
  phi_ayn->addIncoming(b_tnew,  bb_a_dot);
  phi_acomp->addIncoming(b_cnew, bb_a_dot);

  // yi_back = (y_head + A_len - 1 - k) % A_len
  llvm::Value* a_base  = b.CreateAdd(y_head_u64, a_len_u64, "iir_a_base");
  llvm::Value* a_m1    = b.CreateSub(a_base, ci(1), "iir_a_m1");
  llvm::Value* a_sub   = b.CreateSub(a_m1, phi_ak, "iir_a_sub");
  llvm::Value* yi_back = b.CreateURem(a_sub, a_len_u64, "iir_yi_back");

  // a_coef = coef_global[0][B_len + k]
  llvm::Value* a_cidx  = b.CreateAdd(ci(static_cast<int64_t>(B_len)), phi_ak,
                                      "iir_a_cidx");
  llvm::Value* a_cptr  = b.CreateGEP(coef_ty, coef_global, {ci(0), a_cidx},
                                      "iir_a_cptr");
  llvm::Value* a_coef  = b.CreateLoad(f64, a_cptr, "iir_a_coef");

  // y_val = y_ring[yi_back]
  llvm::Value* a_yptr  = b.CreateGEP(f64, y_ring_base, yi_back, "iir_a_yptr");
  llvm::Value* a_yval  = b.CreateLoad(f64, a_yptr, "iir_a_yval");

  // Kahan step (negative): term = -(a_coef * y_val) - comp
  llvm::Value* a_prod  = b.CreateFMul(a_coef, a_yval, "iir_a_prod");
  llvm::Value* a_neg   = b.CreateFNeg(a_prod, "iir_a_neg");
  llvm::Value* a_term  = b.CreateFSub(a_neg, phi_acomp, "iir_a_term");
  llvm::Value* a_tnew  = b.CreateFAdd(phi_ayn, a_term, "iir_a_tnew");
  llvm::Value* a_cdiff = b.CreateFSub(a_tnew, phi_ayn, "iir_a_cdiff");
  llvm::Value* a_cnew  = b.CreateFSub(a_cdiff, a_term, "iir_a_cnew");
  b.CreateBr(bb_a_next);

  b.SetInsertPoint(bb_a_next);
  llvm::Value* ak1     = b.CreateAdd(phi_ak, ci(1), "iir_ak1");
  llvm::Value* ak_done = b.CreateICmpULT(ak1, y_use, "iir_ak_done");
  phi_ak->addIncoming(ak1,    bb_a_next);
  phi_ayn->addIncoming(a_tnew, bb_a_next);
  phi_acomp->addIncoming(a_cnew, bb_a_next);
  b.CreateCondBr(ak_done, bb_a_body, bb_a_exit);

  b.SetInsertPoint(bb_a_exit);
  // Collect y_n: when loop executed, it's a_tnew; when loop was skipped (y_use=0),
  // it's b_tnew. Use a PHI to merge.
  llvm::PHINode* phi_yn_final = b.CreatePHI(f64, 2, "iir_yn_final");
  phi_yn_final->addIncoming(a_tnew,  bb_a_next);   // loop executed at least once
  phi_yn_final->addIncoming(b_tnew,  bb_a_dot);    // loop skipped (y_use == 0)

  // --- 6. y_ring update if A_len > 0 -----------------------------------------
  if (A_len > 0) {
    b.CreateBr(bb_y_upd);

    b.SetInsertPoint(bb_y_upd);
    // y_ring[y_head] = y_n
    llvm::Value* yw_ptr = b.CreateGEP(f64, y_ring_base, y_head_u64, "iir_yw_ptr");
    b.CreateStore(phi_yn_final, yw_ptr);

    // y_head = (y_head + 1) % A_len
    llvm::Value* yh_p1  = b.CreateAdd(y_head_u64, ci(1), "iir_yh_p1");
    llvm::Value* yh_new = b.CreateURem(yh_p1, a_len_u64, "iir_yh_new");
    b.CreateStore(b.CreateUIToFP(yh_new, f64, "iir_yh_new_d"), y_head_ptr);

    // if (y_count < A_len) y_count++
    llvm::Value* a_len_d    = cf(static_cast<double>(A_len));
    llvm::Value* y_nfull    = b.CreateFCmpOLT(y_count_d, a_len_d, "iir_y_nfull");
    llvm::Value* ycnt_inc   = b.CreateFAdd(y_count_d, one_d, "iir_ycnt_inc");
    llvm::Value* y_count_new = b.CreateSelect(y_nfull, ycnt_inc, y_count_d,
                                               "iir_ycnt_new");
    b.CreateStore(y_count_new, y_count_ptr);
    b.CreateBr(bb_y_skip);
  } else {
    b.CreateBr(bb_y_skip);
    // bb_y_upd is unreachable — but it needs a terminator to be valid IR.
    b.SetInsertPoint(bb_y_upd);
    b.CreateUnreachable();
  }

  // --- bb_y_skip: join point; carry y_n forward to merge ---------------------
  b.SetInsertPoint(bb_y_skip);
  llvm::PHINode* phi_yn_skip = b.CreatePHI(f64, 2, "iir_yn_skip");
  if (A_len > 0) {
    phi_yn_skip->addIncoming(phi_yn_final, bb_y_upd);
  } else {
    phi_yn_skip->addIncoming(phi_yn_final, bb_a_exit);
  }
  b.CreateBr(bb_merge);

  // --- merge: PHI for (out_v, emit_flag) ------------------------------------
  b.SetInsertPoint(bb_merge);
  llvm::PHINode* phi_v = b.CreatePHI(f64, 2, "iir_phi_v");
  phi_v->addIncoming(phi_yn_skip, bb_y_skip);
  phi_v->addIncoming(cf(0.0),     bb_warmup);

  llvm::PHINode* phi_flag = b.CreatePHI(i1, 2, "iir_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_y_skip);
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_warmup);

  return StatefulOutput{t, phi_v, phi_flag};
}

}  // namespace rtbot::jit::emit
