#ifndef RTBOT_JIT_EMIT_IDENTITY_H
#define RTBOT_JIT_EMIT_IDENTITY_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Stateless 1->1 passthrough: emits (t, v) unchanged. Used for both Identity
// and BooleanToNumber (the latter is a no-op at the JIT level because the
// upstream Boolean-producing op already emits 0.0/1.0).
inline llvm::Value* emit_identity(IrEmissionContext&, llvm::Value* v) {
  return v;
}

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_IDENTITY_H
