#include "rtbot/compiled/jit/emit/Comparison.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

#include "rtbot/compiled/jit/emit/Transcendental.h"

namespace rtbot::jit::emit {

namespace {

// Returns the 1.0 / 0.0 double constants used by all comparison emitters.
llvm::Value* one(llvm::IRBuilder<>& bld) {
  return llvm::ConstantFP::get(bld.getDoubleTy(), 1.0);
}
llvm::Value* zero(llvm::IRBuilder<>& bld) {
  return llvm::ConstantFP::get(bld.getDoubleTy(), 0.0);
}

}  // namespace

llvm::Value* emit_gt(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* cmp = bld.CreateFCmpOGT(a, b);
  return bld.CreateSelect(cmp, one(bld), zero(bld));
}

llvm::Value* emit_gte(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* cmp = bld.CreateFCmpOGE(a, b);
  return bld.CreateSelect(cmp, one(bld), zero(bld));
}

llvm::Value* emit_lt(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* cmp = bld.CreateFCmpOLT(a, b);
  return bld.CreateSelect(cmp, one(bld), zero(bld));
}

llvm::Value* emit_lte(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* cmp = bld.CreateFCmpOLE(a, b);
  return bld.CreateSelect(cmp, one(bld), zero(bld));
}

llvm::Value* emit_eq(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* cmp = bld.CreateFCmpOEQ(a, b);
  return bld.CreateSelect(cmp, one(bld), zero(bld));
}

llvm::Value* emit_neq(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* cmp = bld.CreateFCmpONE(a, b);
  return bld.CreateSelect(cmp, one(bld), zero(bld));
}

llvm::Value* emit_eq_tol(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b,
                          double tol) {
  auto& bld = ec.b();
  llvm::Value* diff   = bld.CreateFSub(a, b);
  llvm::Value* abs_d  = emit_abs(ec, diff);
  llvm::Value* tol_v  = llvm::ConstantFP::get(bld.getDoubleTy(), tol);
  llvm::Value* cmp    = bld.CreateFCmpOLE(abs_d, tol_v);
  return bld.CreateSelect(cmp, one(bld), zero(bld));
}

llvm::Value* emit_neq_tol(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b,
                           double tol) {
  auto& bld = ec.b();
  llvm::Value* diff   = bld.CreateFSub(a, b);
  llvm::Value* abs_d  = emit_abs(ec, diff);
  llvm::Value* tol_v  = llvm::ConstantFP::get(bld.getDoubleTy(), tol);
  llvm::Value* cmp    = bld.CreateFCmpOGT(abs_d, tol_v);
  return bld.CreateSelect(cmp, one(bld), zero(bld));
}

}  // namespace rtbot::jit::emit
