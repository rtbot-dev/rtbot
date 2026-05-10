// FIR.cpp
//
// IR emission for one FIR (Finite Impulse Response) filter step.
// Mirrors FE FIR_UPDATE (FusedScalarEval.h case 42).
//
// State layout (state_offset + W + 2 doubles):
//   [0..W-1]  ring buffer
//   [W]       head — next write position (stored as double, read as size_t)
//   [W+1]     count (double)
//
// Coefficients are baked in as a private global constant array at emit time,
// so per-tick execution needs no extra parameter.

#include "rtbot/compiled/jit/emit/FIR.h"

#include <string>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

StatefulOutput emit_fir(IrEmissionContext& ec,
                        std::size_t state_offset,
                        std::size_t W,
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

  // --- Bake coefficients into a private global constant array ----------------
  // Use state_offset in the name so multiple FIR ops in the same module get
  // distinct globals.
  auto* coef_ty   = llvm::ArrayType::get(f64, W);
  std::vector<llvm::Constant*> coef_consts;
  coef_consts.reserve(W);
  for (double c : coefficients)
    coef_consts.push_back(llvm::ConstantFP::get(f64, c));
  auto* coef_init = llvm::ConstantArray::get(coef_ty, coef_consts);
  std::string global_name = "fir_coeffs_" + std::to_string(state_offset);
  auto* coef_global = new llvm::GlobalVariable(
      ec.mod(), coef_ty, /*isConstant=*/true,
      llvm::GlobalValue::PrivateLinkage, coef_init, global_name);

  // --- State slot pointers ---------------------------------------------------
  const std::size_t IDX_HEAD  = state_offset + W;
  const std::size_t IDX_COUNT = state_offset + W + 1;

  llvm::Value* ring_base  = ec.state_gep(state_offset);
  llvm::Value* head_ptr   = ec.state_gep(IDX_HEAD);
  llvm::Value* count_ptr  = ec.state_gep(IDX_COUNT);

  llvm::Value* w_d   = cf(static_cast<double>(W));
  llvm::Value* w_u64 = ci(static_cast<int64_t>(W));
  llvm::Value* one_d = cf(1.0);

  // --- Load head and count; compute ring write index -------------------------
  llvm::Value* head_d   = b.CreateLoad(f64, head_ptr,  "fir_head_d");
  llvm::Value* idx_u64  = b.CreateFPToUI(head_d, i64,  "fir_idx");
  llvm::Value* count_d  = b.CreateLoad(f64, count_ptr, "fir_count_d");

  // ring[idx] = v
  llvm::Value* ring_ptr_w = b.CreateGEP(f64, ring_base, idx_u64, "fir_ring_w");
  b.CreateStore(v, ring_ptr_w);

  // head = (idx + 1) % W  (stored back as double)
  llvm::Value* idx_p1      = b.CreateAdd(idx_u64, ci(1),   "fir_idx_p1");
  llvm::Value* head_new_u  = b.CreateURem(idx_p1, w_u64,   "fir_head_new_u");
  llvm::Value* head_new_d  = b.CreateUIToFP(head_new_u, f64, "fir_head_new_d");
  b.CreateStore(head_new_d, head_ptr);

  // count++
  llvm::Value* count_new = b.CreateFAdd(count_d, one_d, "fir_count_new");
  b.CreateStore(count_new, count_ptr);

  // --- Warmup gate -----------------------------------------------------------
  llvm::Function* fn = b.GetInsertBlock()->getParent();

  llvm::BasicBlock* bb_warmup = llvm::BasicBlock::Create(ctx, "fir_warmup",  fn);
  llvm::BasicBlock* bb_dot    = llvm::BasicBlock::Create(ctx, "fir_dot_hdr", fn);
  llvm::BasicBlock* bb_body   = llvm::BasicBlock::Create(ctx, "fir_dot_body",fn);
  llvm::BasicBlock* bb_next   = llvm::BasicBlock::Create(ctx, "fir_dot_next",fn);
  llvm::BasicBlock* bb_exit   = llvm::BasicBlock::Create(ctx, "fir_dot_exit",fn);
  llvm::BasicBlock* bb_merge  = llvm::BasicBlock::Create(ctx, "fir_merge",   fn);

  llvm::Value* cond_warm = b.CreateFCmpOLT(count_new, w_d, "fir_cond_warm");
  b.CreateCondBr(cond_warm, bb_warmup, bb_dot);

  // --- warmup: no output -----------------------------------------------------
  b.SetInsertPoint(bb_warmup);
  b.CreateBr(bb_merge);

  // --- dot product loop (counted i64, k = 0..W-1) ----------------------------
  // result += coeffs[k] * ring[(idx + W - k) % W]
  //
  // Note: idx here is the position *just written* (before head advance).
  // (idx + W - k) % W for k=0 gives idx → newest sample (coeff[0]).

  b.SetInsertPoint(bb_dot);
  b.CreateBr(bb_body);  // fall into header with k=0, acc=0.0

  // --- loop header (PHI) -----------------------------------------------------
  b.SetInsertPoint(bb_body);
  llvm::PHINode* phi_k   = b.CreatePHI(i64, 2, "fir_k");
  llvm::PHINode* phi_acc = b.CreatePHI(f64, 2, "fir_acc");
  phi_k->addIncoming(ci(0),    bb_dot);
  phi_acc->addIncoming(cf(0.0),bb_dot);

  // ring_idx = (idx + W - k) % W
  llvm::Value* base_off  = b.CreateAdd(idx_u64, w_u64, "fir_base_off");
  llvm::Value* sub_k     = b.CreateSub(base_off, phi_k, "fir_sub_k");
  llvm::Value* ring_idx  = b.CreateURem(sub_k,   w_u64, "fir_ring_idx");

  // coeff_ptr = GEP into the global array at index k
  llvm::Value* coeff_ptr = b.CreateGEP(coef_ty, coef_global,
                                        {ci(0), phi_k}, "fir_coeff_ptr");
  llvm::Value* coeff_v   = b.CreateLoad(f64, coeff_ptr, "fir_coeff");

  // ring_v = ring_base[ring_idx]
  llvm::Value* ring_r    = b.CreateGEP(f64, ring_base, ring_idx, "fir_ring_r");
  llvm::Value* ring_v    = b.CreateLoad(f64, ring_r, "fir_ring_v");

  // acc += coeff * ring_v
  llvm::Value* prod      = b.CreateFMul(coeff_v, ring_v, "fir_prod");
  llvm::Value* acc_new   = b.CreateFAdd(phi_acc, prod,   "fir_acc_new");
  b.CreateBr(bb_next);

  // --- loop latch: advance k, loop-back or exit ------------------------------
  b.SetInsertPoint(bb_next);
  llvm::Value* k1        = b.CreateAdd(phi_k, ci(1), "fir_k1");
  llvm::Value* k_lt_w    = b.CreateICmpULT(k1, w_u64, "fir_k_lt_w");
  phi_k->addIncoming(k1,      bb_next);
  phi_acc->addIncoming(acc_new, bb_next);
  b.CreateCondBr(k_lt_w, bb_body, bb_exit);

  // --- dot product exit block ------------------------------------------------
  b.SetInsertPoint(bb_exit);
  b.CreateBr(bb_merge);

  // --- merge: PHI for (out_v, emit_flag) -------------------------------------
  b.SetInsertPoint(bb_merge);

  llvm::PHINode* phi_v    = b.CreatePHI(f64, 2, "fir_phi_v");
  phi_v->addIncoming(acc_new, bb_exit);
  phi_v->addIncoming(cf(0.0), bb_warmup);

  llvm::PHINode* phi_flag = b.CreatePHI(i1, 2, "fir_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_exit);
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_warmup);

  return StatefulOutput{t, phi_v, phi_flag};
}

}  // namespace rtbot::jit::emit
