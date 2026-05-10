#include "rtbot/compiled/jit/emit/Constant.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

llvm::Value* emit_constant(IrEmissionContext& ec, double value) {
  return llvm::ConstantFP::get(ec.b().getDoubleTy(), value);
}

}  // namespace rtbot::jit::emit
