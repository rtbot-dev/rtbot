#ifndef RTBOT_JIT_EMIT_TIME_SHIFT_H
#define RTBOT_JIT_EMIT_TIME_SHIFT_H

#include <cstdint>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // for StatefulOutput

namespace rtbot::jit::emit {

// Stateless 1->1 with conditional skip: emits (t + shift, v). Skips the
// emission entirely when t + shift < 0 (the FE interpreter throws on a
// negative shifted timestamp; the JIT silently drops to keep the stateless
// fast-path predictable).
//
// t must be i64; v must be double.
StatefulOutput emit_time_shift(IrEmissionContext& ec, std::int64_t shift,
                                llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_TIME_SHIFT_H
