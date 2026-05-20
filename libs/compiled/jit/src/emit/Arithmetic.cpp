#include "rtbot/compiled/jit/emit/Arithmetic.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

#include "rtbot/compiled/jit/emit/Transcendental.h"

namespace rtbot::jit::emit {

llvm::Value* emit_add(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  return ec.b().CreateFAdd(a, b);
}

llvm::Value* emit_sub(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  return ec.b().CreateFSub(a, b);
}

llvm::Value* emit_mul(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  return ec.b().CreateFMul(a, b);
}

llvm::Value* emit_div(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b) {
  return ec.b().CreateFDiv(a, b);
}

llvm::Value* emit_scale(IrEmissionContext& ec, llvm::Value* v, double k) {
  return ec.b().CreateFMul(v, llvm::ConstantFP::get(ec.b().getDoubleTy(), k));
}

llvm::Value* emit_add_scalar(IrEmissionContext& ec, llvm::Value* v, double k) {
  return ec.b().CreateFAdd(v, llvm::ConstantFP::get(ec.b().getDoubleTy(), k));
}

llvm::Value* emit_power_scalar(IrEmissionContext& ec, llvm::Value* v, double k) {
  return emit_pow(ec, v, llvm::ConstantFP::get(ec.b().getDoubleTy(), k));
}

}  // namespace rtbot::jit::emit
