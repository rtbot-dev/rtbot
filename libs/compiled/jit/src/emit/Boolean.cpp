#include "rtbot/compiled/jit/emit/Boolean.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

namespace {

llvm::Value* one(llvm::IRBuilder<>& bld) {
  return llvm::ConstantFP::get(bld.getDoubleTy(), 1.0);
}
llvm::Value* zero(llvm::IRBuilder<>& bld) {
  return llvm::ConstantFP::get(bld.getDoubleTy(), 0.0);
}

}  // namespace

llvm::Value* emit_and(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* a_nz = bld.CreateFCmpONE(a, zero(bld));
  llvm::Value* b_nz = bld.CreateFCmpONE(b, zero(bld));
  llvm::Value* both = bld.CreateAnd(a_nz, b_nz);
  return bld.CreateSelect(both, one(bld), zero(bld));
}

llvm::Value* emit_or(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* a_nz = bld.CreateFCmpONE(a, zero(bld));
  llvm::Value* b_nz = bld.CreateFCmpONE(b, zero(bld));
  llvm::Value* either = bld.CreateOr(a_nz, b_nz);
  return bld.CreateSelect(either, one(bld), zero(bld));
}

llvm::Value* emit_not(IrEmissionContext& ec, llvm::Value* v) {
  auto& bld = ec.b();
  llvm::Value* is_zero = bld.CreateFCmpOEQ(v, zero(bld));
  return bld.CreateSelect(is_zero, one(bld), zero(bld));
}

llvm::Value* emit_xor(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* a_nz = bld.CreateFCmpONE(a, zero(bld));
  llvm::Value* b_nz = bld.CreateFCmpONE(b, zero(bld));
  llvm::Value* x    = bld.CreateXor(a_nz, b_nz);
  return bld.CreateSelect(x, one(bld), zero(bld));
}

// FE LogicalNand uses ReduceJoin with init=true and combine(acc,x) = !(acc && x).
// For 2 inputs the fold becomes: combine(combine(true, a), b) = !(!a && b)
//                                                            = a || !b.
llvm::Value* emit_nand(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* a_nz   = bld.CreateFCmpONE(a, zero(bld));
  llvm::Value* b_nz   = bld.CreateFCmpONE(b, zero(bld));
  llvm::Value* not_b  = bld.CreateNot(b_nz);
  llvm::Value* res    = bld.CreateOr(a_nz, not_b);
  return bld.CreateSelect(res, one(bld), zero(bld));
}

// FE LogicalNor uses ReduceJoin with init=true and combine(acc,x) = !(acc || x).
// For 2 inputs the fold becomes: combine(combine(true, a), b) = !(false || b) = !b.
llvm::Value* emit_nor(IrEmissionContext& ec, llvm::Value* /*a*/, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* b_nz  = bld.CreateFCmpONE(b, zero(bld));
  llvm::Value* not_b = bld.CreateNot(b_nz);
  return bld.CreateSelect(not_b, one(bld), zero(bld));
}

// FE LogicalXnor: ReduceJoin with NO init. For 2 inputs: combine(a, b) = (a == b).
llvm::Value* emit_xnor(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* a_nz = bld.CreateFCmpONE(a, zero(bld));
  llvm::Value* b_nz = bld.CreateFCmpONE(b, zero(bld));
  llvm::Value* eq   = bld.CreateICmpEQ(a_nz, b_nz);
  return bld.CreateSelect(eq, one(bld), zero(bld));
}

// Implication: !a || b
llvm::Value* emit_implication(IrEmissionContext& ec, llvm::Value* a,
                               llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* a_nz   = bld.CreateFCmpONE(a, zero(bld));
  llvm::Value* b_nz   = bld.CreateFCmpONE(b, zero(bld));
  llvm::Value* not_a  = bld.CreateNot(a_nz);
  llvm::Value* res    = bld.CreateOr(not_a, b_nz);
  return bld.CreateSelect(res, one(bld), zero(bld));
}

}  // namespace rtbot::jit::emit
