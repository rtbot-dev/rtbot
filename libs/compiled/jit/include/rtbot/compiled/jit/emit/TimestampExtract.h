#ifndef RTBOT_JIT_EMIT_TIMESTAMP_EXTRACT_H
#define RTBOT_JIT_EMIT_TIMESTAMP_EXTRACT_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Stateless 1->1: emits (t, static_cast<double>(t)). Input value is ignored.
// t must be i64.
llvm::Value* emit_timestamp_extract(IrEmissionContext& ec, llvm::Value* t);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_TIMESTAMP_EXTRACT_H
