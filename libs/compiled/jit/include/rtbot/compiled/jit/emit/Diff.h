#ifndef RTBOT_JIT_EMIT_DIFF_H
#define RTBOT_JIT_EMIT_DIFF_H

#include <cstddef>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // for StatefulOutput

namespace rtbot::jit::emit {

// Stateful 2-sample diff: emits v - prev_v on every input from the second
// onward. Mirrors rtbot::compiled::DiffStage.
//
// State layout (4 doubles at state_offset):
//   state[0] — prev_v  (double)
//   state[1] — prev_t  (i64 bitcast to double)
//   state[2] — curr_t  (unused, kept for compatibility)
//   state[3] — count   (as double)
//
// use_oldest_time: if true, out_t = t (current); if false, out_t = prev_t.
// t must be i64; v must be double.
StatefulOutput emit_diff(IrEmissionContext& ec, std::size_t state_offset,
                         bool use_oldest_time,
                         llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_DIFF_H
