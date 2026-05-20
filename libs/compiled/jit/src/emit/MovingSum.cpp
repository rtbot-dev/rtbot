// MovingSum.cpp
//
// IR emission for one MovingSum<W> step. Identical to MovingAverage except
// the emitted value is the windowed sum (no divide by W at the end).
// Mirrors FE MSUM_UPDATE (FusedScalarEval.h case 36).
//
// State layout (state_offset + W+3 doubles):
//   [0..W-1]  ring buffer
//   [W]       Kahan sum
//   [W+1]     Kahan compensation
//   [W+2]     count (double)

#include "rtbot/compiled/jit/emit/MovingSum.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

StatefulOutput emit_moving_sum(IrEmissionContext& ec,
                               std::size_t state_offset,
                               std::size_t W,
                               llvm::Value* t, llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Type* i1  = llvm::Type::getInt1Ty(ctx);

  // Constant helpers.
  auto ci = [&](int64_t val) -> llvm::Value* {
    return llvm::ConstantInt::get(i64, val);
  };
  auto cf = [&](double val) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, val);
  };

  // State slot indices (absolute into the flat state buffer).
  const std::size_t IDX_SUM   = state_offset + W;
  const std::size_t IDX_COMP  = state_offset + W + 1;
  const std::size_t IDX_COUNT = state_offset + W + 2;

  // GEP helpers using IrEmissionContext::state_gep for static offsets.
  auto sum_ptr   = ec.state_gep(IDX_SUM);
  auto comp_ptr  = ec.state_gep(IDX_COMP);
  auto count_ptr = ec.state_gep(IDX_COUNT);

  // Dynamic ring-buffer GEP: state_ptr + (state_offset + idx).
  auto ring_base = ec.state_gep(state_offset);  // ptr to state[state_offset]

  llvm::Value* w_d   = cf(static_cast<double>(W));
  llvm::Value* w_u64 = ci(static_cast<int64_t>(W));
  llvm::Value* one_d = cf(1.0);

  // Load count.
  llvm::Value* count_d   = b.CreateLoad(f64, count_ptr, "ms_count_d");
  llvm::Value* count_u64 = b.CreateFPToUI(count_d, i64, "ms_count_u64");
  llvm::Value* idx_u64   = b.CreateURem(count_u64, w_u64, "ms_idx");

  // cond_sub: count_d >= W — need to subtract the leaving element.
  llvm::Value* cond_sub = b.CreateFCmpOGE(count_d, w_d, "ms_cond_sub");

  // Retrieve the parent function to create basic blocks.
  llvm::Function* fn = b.GetInsertBlock()->getParent();

  llvm::BasicBlock* bb_sub   = llvm::BasicBlock::Create(ctx, "ms_kahan_sub", fn);
  llvm::BasicBlock* bb_add   = llvm::BasicBlock::Create(ctx, "ms_kahan_add", fn);
  llvm::BasicBlock* bb_check = llvm::BasicBlock::Create(ctx, "ms_emit_check", fn);
  llvm::BasicBlock* bb_true  = llvm::BasicBlock::Create(ctx, "ms_emit_true",  fn);
  llvm::BasicBlock* bb_false = llvm::BasicBlock::Create(ctx, "ms_emit_false", fn);
  llvm::BasicBlock* bb_merge = llvm::BasicBlock::Create(ctx, "ms_merge",      fn);

  b.CreateCondBr(cond_sub, bb_sub, bb_add);

  // --- kahan_sub: remove leaving element -----------------------------------
  b.SetInsertPoint(bb_sub);
  {
    llvm::Value* ring_ptr = b.CreateGEP(f64, ring_base, idx_u64, "ms_ring_sub");
    llvm::Value* leaving  = b.CreateLoad(f64, ring_ptr, "ms_leaving");
    ec.emit_kahan_subtract(sum_ptr, comp_ptr, leaving);
  }
  b.CreateBr(bb_add);

  // --- kahan_add: store v in ring, add v to sum ----------------------------
  b.SetInsertPoint(bb_add);
  {
    llvm::Value* ring_ptr = b.CreateGEP(f64, ring_base, idx_u64, "ms_ring_add");
    b.CreateStore(v, ring_ptr);
    ec.emit_kahan_add(sum_ptr, comp_ptr, v);
  }
  // ++count
  llvm::Value* count_new = b.CreateFAdd(count_d, one_d, "ms_count_new");
  b.CreateStore(count_new, count_ptr);
  b.CreateBr(bb_check);

  // --- emit_check: count_new < W → warmup (no emit) ------------------------
  b.SetInsertPoint(bb_check);
  llvm::Value* cond_warm = b.CreateFCmpOLT(count_new, w_d, "ms_cond_warm");
  b.CreateCondBr(cond_warm, bb_false, bb_true);

  // --- emit_true: out_v = sum (no divide — MovingSum emits the raw sum) ----
  b.SetInsertPoint(bb_true);
  llvm::Value* sum_final  = b.CreateLoad(f64, sum_ptr, "ms_sum_final");
  llvm::Value* out_v_true = sum_final;  // sum, not sum/W
  b.CreateBr(bb_merge);

  // --- emit_false: no output -----------------------------------------------
  b.SetInsertPoint(bb_false);
  b.CreateBr(bb_merge);

  // --- merge: PHI for (out_v, emit_flag) -----------------------------------
  b.SetInsertPoint(bb_merge);

  llvm::PHINode* phi_v    = b.CreatePHI(f64, 2, "ms_phi_v");
  phi_v->addIncoming(out_v_true, bb_true);
  phi_v->addIncoming(cf(0.0),    bb_false);

  llvm::PHINode* phi_flag = b.CreatePHI(i1, 2, "ms_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_true);
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_false);

  return StatefulOutput{t, phi_v, phi_flag};
}

}  // namespace rtbot::jit::emit
