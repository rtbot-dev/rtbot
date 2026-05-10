#ifndef RTBOT_JIT_EMIT_CONSTANT_H
#define RTBOT_JIT_EMIT_CONSTANT_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Stateless 1->1: ignores the input value and emits the configured constant.
// The input timestamp is preserved by the caller.
llvm::Value* emit_constant(IrEmissionContext& ec, double value);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_CONSTANT_H
