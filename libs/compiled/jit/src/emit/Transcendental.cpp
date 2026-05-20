#include "rtbot/compiled/jit/emit/Transcendental.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

namespace {

// Materialize a unary double -> double LLVM intrinsic declaration.
llvm::Function* unary_intrinsic(IrEmissionContext& ec, llvm::Intrinsic::ID id) {
  llvm::Type* dbl = ec.b().getDoubleTy();
  return llvm::Intrinsic::getDeclaration(&ec.mod(), id,
                                         llvm::ArrayRef<llvm::Type*>{dbl});
}

// Materialize a binary (double, double) -> double LLVM intrinsic declaration.
llvm::Function* binary_intrinsic(IrEmissionContext& ec, llvm::Intrinsic::ID id) {
  llvm::Type* dbl = ec.b().getDoubleTy();
  return llvm::Intrinsic::getDeclaration(&ec.mod(), id,
                                         llvm::ArrayRef<llvm::Type*>{dbl});
}

}  // namespace

llvm::Value* emit_pow(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  auto* fn = binary_intrinsic(ec, llvm::Intrinsic::pow);
  return ec.b().CreateCall(fn, {a, b});
}

llvm::Value* emit_abs(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::fabs);
  return ec.b().CreateCall(fn, {v});
}

llvm::Value* emit_sqrt(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::sqrt);
  return ec.b().CreateCall(fn, {v});
}

llvm::Value* emit_log(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::log);
  return ec.b().CreateCall(fn, {v});
}

llvm::Value* emit_log10(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::log10);
  return ec.b().CreateCall(fn, {v});
}

llvm::Value* emit_exp(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::exp);
  return ec.b().CreateCall(fn, {v});
}

llvm::Value* emit_sin(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::sin);
  return ec.b().CreateCall(fn, {v});
}

llvm::Value* emit_cos(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::cos);
  return ec.b().CreateCall(fn, {v});
}

// No direct LLVM intrinsic for tan. Declare and call the system libm `tan`
// directly so the JIT path invokes the exact same function as the FE
// interpreter's std::tan, giving bit-exact parity. Using sin/cos intrinsics
// divided together would introduce an extra rounding step (1 ULP drift).
llvm::Value* emit_tan(IrEmissionContext& ec, llvm::Value* v) {
  auto& bld = ec.b();
  llvm::Type* dbl = bld.getDoubleTy();
  auto callee = ec.mod().getOrInsertFunction(
      "tan", llvm::FunctionType::get(dbl, {dbl}, /*isVarArg=*/false));
  return bld.CreateCall(callee, {v});
}

// (v > 0) ? 1.0 : (v < 0) ? -1.0 : 0.0 — mirrors FusedScalarEval SIGN.
llvm::Value* emit_sign(IrEmissionContext& ec, llvm::Value* v) {
  auto& bld = ec.b();
  llvm::Type* dbl = bld.getDoubleTy();
  llvm::Value* zero = llvm::ConstantFP::get(dbl, 0.0);
  llvm::Value* one = llvm::ConstantFP::get(dbl, 1.0);
  llvm::Value* neg_one = llvm::ConstantFP::get(dbl, -1.0);
  llvm::Value* pos = bld.CreateFCmpOGT(v, zero);
  llvm::Value* neg = bld.CreateFCmpOLT(v, zero);
  llvm::Value* neg_or_zero = bld.CreateSelect(neg, neg_one, zero);
  return bld.CreateSelect(pos, one, neg_or_zero);
}

llvm::Value* emit_floor(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::floor);
  return ec.b().CreateCall(fn, {v});
}

llvm::Value* emit_ceil(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::ceil);
  return ec.b().CreateCall(fn, {v});
}

llvm::Value* emit_round(IrEmissionContext& ec, llvm::Value* v) {
  auto* fn = unary_intrinsic(ec, llvm::Intrinsic::round);
  return ec.b().CreateCall(fn, {v});
}

llvm::Value* emit_neg(IrEmissionContext& ec, llvm::Value* v) {
  return ec.b().CreateFNeg(v);
}

}  // namespace rtbot::jit::emit
