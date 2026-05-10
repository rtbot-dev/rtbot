// Diff.cpp
//
// IR emission for one Diff step. Mirrors DiffStage::process from
// libs/compiled/include/rtbot/compiled/DiffStage.h exactly.
//
// State layout (state_offset + 4 doubles):
//   [0] prev_v  — previous sample value
//   [1] prev_t  — previous timestamp (bit-cast i64 <-> double)
//   [2] curr_t  — unused slot (kept for layout compatibility)
//   [3] count   — sample count (stored as double)
//
// Algorithm:
//   count = load state[3], cast to i64
//   if count == 0:
//     store v    -> state[0]
//     store t    -> state[1]  (bitcast to double)
//     store 1.0  -> state[3]
//     emit_flag = false
//   else:
//     prev_v = load state[0]
//     prev_t = load state[1]  (bitcast to i64)
//     out_v  = v - prev_v
//     out_t  = use_oldest_time ? t : prev_t
//     store v                 -> state[0]
//     store t (bitcast)       -> state[1]
//     store count + 1 (as f64)-> state[3]
//     emit_flag = true

#include "rtbot/compiled/jit/emit/Diff.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

StatefulOutput emit_diff(IrEmissionContext& ec, std::size_t state_offset,
                         bool use_oldest_time,
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

  // Absolute state-slot pointers.
  llvm::Value* prev_v_ptr = ec.state_gep(state_offset + 0);
  llvm::Value* prev_t_ptr = ec.state_gep(state_offset + 1);
  // state_offset + 2 is curr_t (unused)
  llvm::Value* count_ptr  = ec.state_gep(state_offset + 3);

  // Load count and check if this is the first sample.
  llvm::Value* count_d   = b.CreateLoad(f64, count_ptr, "df_count_d");
  llvm::Value* count_i64 = b.CreateFPToUI(count_d, i64, "df_count_i64");
  llvm::Value* is_first  = b.CreateICmpEQ(count_i64, ci(0), "df_is_first");

  llvm::Function* fn = b.GetInsertBlock()->getParent();

  llvm::BasicBlock* bb_init  = llvm::BasicBlock::Create(ctx, "df_init",  fn);
  llvm::BasicBlock* bb_emit  = llvm::BasicBlock::Create(ctx, "df_emit",  fn);
  llvm::BasicBlock* bb_merge = llvm::BasicBlock::Create(ctx, "df_merge", fn);

  b.CreateCondBr(is_first, bb_init, bb_emit);

  // --- init_bb: first sample, store and skip output -----------------------
  b.SetInsertPoint(bb_init);
  b.CreateStore(v, prev_v_ptr);
  b.CreateStore(b.CreateBitCast(t, f64, "df_t_bc_init"), prev_t_ptr);
  b.CreateStore(cf(1.0), count_ptr);
  b.CreateBr(bb_merge);

  // --- emit_bb: subsequent samples, compute diff --------------------------
  b.SetInsertPoint(bb_emit);
  llvm::Value* prev_v    = b.CreateLoad(f64, prev_v_ptr, "df_prev_v");
  llvm::Value* prev_t_d  = b.CreateLoad(f64, prev_t_ptr, "df_prev_t_d");
  llvm::Value* prev_t_i  = b.CreateBitCast(prev_t_d, i64, "df_prev_t_i");

  llvm::Value* out_v_val = b.CreateFSub(v, prev_v, "df_out_v");
  llvm::Value* out_t_val = use_oldest_time ? t : prev_t_i;

  // Advance state.
  b.CreateStore(v, prev_v_ptr);
  b.CreateStore(b.CreateBitCast(t, f64, "df_t_bc_emit"), prev_t_ptr);
  llvm::Value* count_new = b.CreateFAdd(count_d, cf(1.0), "df_count_new");
  b.CreateStore(count_new, count_ptr);
  b.CreateBr(bb_merge);

  // --- merge_bb: PHI nodes ------------------------------------------------
  b.SetInsertPoint(bb_merge);

  llvm::PHINode* phi_flag  = b.CreatePHI(i1,  2, "df_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_init);
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_emit);

  llvm::PHINode* phi_out_t = b.CreatePHI(i64, 2, "df_phi_out_t");
  phi_out_t->addIncoming(ci(0),       bb_init);
  phi_out_t->addIncoming(out_t_val,   bb_emit);

  llvm::PHINode* phi_out_v = b.CreatePHI(f64, 2, "df_phi_out_v");
  phi_out_v->addIncoming(cf(0.0),    bb_init);
  phi_out_v->addIncoming(out_v_val,  bb_emit);

  return StatefulOutput{phi_out_t, phi_out_v, phi_flag};
}

}  // namespace rtbot::jit::emit
