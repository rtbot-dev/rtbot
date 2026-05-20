// SignChange.cpp
//
// IR emission for one SIGN_CHANGE step. Mirrors FE case 39 from
// libs/fuse/include/rtbot/fuse/FusedScalarEval.h exactly.
//
// State layout (2 doubles at state_offset):
//   [0] prev_v    — previous sample value
//   [1] has_prev  — 0.0 = no prior sample, 1.0 = prior sample exists
//
// Algorithm:
//   has_prev = load state[1]
//   was_first = (has_prev == 0.0)
//   prev_v = load state[0]   // load before storing v
//   store v   -> state[0]
//   store 1.0 -> state[1]
//   if was_first:
//     emit_flag = false, out_v = 0.0
//   else:
//     d = v - prev_v
//     out_v = (d > 0.0) ? 1.0 : (d < 0.0) ? -1.0 : 0.0
//     emit_flag = true
//   out_t = t (gated by emit_flag)

#include "rtbot/compiled/jit/emit/SignChange.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

StatefulOutput emit_sign_change(IrEmissionContext& ec, std::size_t state_offset,
                                llvm::Value* t, llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i1  = llvm::Type::getInt1Ty(ctx);

  auto cf = [&](double val) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, val);
  };

  llvm::Value* prev_v_ptr   = ec.state_gep(state_offset + 0);
  llvm::Value* has_prev_ptr = ec.state_gep(state_offset + 1);

  // Load has_prev and determine if this is the first sample.
  llvm::Value* has_prev  = b.CreateLoad(f64, has_prev_ptr, "sc_has_prev");
  llvm::Value* was_first = b.CreateFCmpOEQ(has_prev, cf(0.0), "sc_was_first");

  // Load prev_v before we overwrite state[0].
  llvm::Value* prev_v = b.CreateLoad(f64, prev_v_ptr, "sc_prev_v");

  // Unconditionally advance state.
  b.CreateStore(v, prev_v_ptr);
  b.CreateStore(cf(1.0), has_prev_ptr);

  llvm::Function* fn = b.GetInsertBlock()->getParent();

  llvm::BasicBlock* bb_first = llvm::BasicBlock::Create(ctx, "sc_first",  fn);
  llvm::BasicBlock* bb_emit  = llvm::BasicBlock::Create(ctx, "sc_emit",   fn);
  llvm::BasicBlock* bb_merge = llvm::BasicBlock::Create(ctx, "sc_merge",  fn);

  b.CreateCondBr(was_first, bb_first, bb_emit);

  // --- first_bb: suppress output --------------------------------------------
  b.SetInsertPoint(bb_first);
  b.CreateBr(bb_merge);

  // --- emit_bb: compute sign(v - prev_v) ------------------------------------
  b.SetInsertPoint(bb_emit);
  llvm::Value* d     = b.CreateFSub(v, prev_v, "sc_d");
  llvm::Value* is_pos = b.CreateFCmpOGT(d, cf(0.0), "sc_is_pos");
  llvm::Value* is_neg = b.CreateFCmpOLT(d, cf(0.0), "sc_is_neg");
  llvm::Value* neg_or_zero = b.CreateSelect(is_neg, cf(-1.0), cf(0.0), "sc_neg_or_zero");
  llvm::Value* out_v_val   = b.CreateSelect(is_pos, cf(1.0), neg_or_zero, "sc_out_v");
  b.CreateBr(bb_merge);

  // --- merge_bb: PHI nodes --------------------------------------------------
  b.SetInsertPoint(bb_merge);

  llvm::PHINode* phi_flag = b.CreatePHI(i1,  2, "sc_phi_flag");
  phi_flag->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_first);
  phi_flag->addIncoming(llvm::ConstantInt::getTrue(ctx),  bb_emit);

  llvm::PHINode* phi_out_v = b.CreatePHI(f64, 2, "sc_phi_out_v");
  phi_out_v->addIncoming(cf(0.0),     bb_first);
  phi_out_v->addIncoming(out_v_val,   bb_emit);

  // Timestamp is unconditional (caller uses emit_flag to gate use).
  return StatefulOutput{t, phi_out_v, phi_flag};
}

}  // namespace rtbot::jit::emit
