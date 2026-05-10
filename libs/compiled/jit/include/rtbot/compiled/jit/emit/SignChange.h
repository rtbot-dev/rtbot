#ifndef RTBOT_JIT_EMIT_SIGN_CHANGE_H
#define RTBOT_JIT_EMIT_SIGN_CHANGE_H

#include <cstddef>
#include <cstdint>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // For StatefulOutput

namespace rtbot::jit::emit {

// Stateful 2-sample sign-of-delta emitter. State: prev_v + has_prev.
// On each input: emit sign(v - prev) if has_prev != 0, else emit_flag = false.
// State is unconditionally advanced (prev_v = v, has_prev = 1.0).
// Mirrors FE SIGN_CHANGE bytecode opcode (FusedScalarEval.h case 39).
StatefulOutput emit_sign_change(IrEmissionContext& ec, std::size_t state_offset,
                                llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_SIGN_CHANGE_H
