#include "rtbot/compiled/jit/emit/TimestampExtract.h"

#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

llvm::Value* emit_timestamp_extract(IrEmissionContext& ec, llvm::Value* t) {
  return ec.b().CreateSIToFP(t, ec.b().getDoubleTy());
}

}  // namespace rtbot::jit::emit
