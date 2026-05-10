#ifndef RTBOT_JIT_EMIT_BOOLEAN_TO_NUMBER_H
#define RTBOT_JIT_EMIT_BOOLEAN_TO_NUMBER_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// At the JIT level all values are already doubles; the upstream
// Boolean-producing op already emitted 0.0/1.0. So this is a passthrough.
inline llvm::Value* emit_boolean_to_number(IrEmissionContext&, llvm::Value* v) {
  return v;
}

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_BOOLEAN_TO_NUMBER_H
