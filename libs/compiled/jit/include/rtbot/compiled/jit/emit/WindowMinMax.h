#ifndef RTBOT_JIT_EMIT_WINDOW_MIN_MAX_H
#define RTBOT_JIT_EMIT_WINDOW_MIN_MAX_H

#include <cstddef>
#include <cstdint>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // For StatefulOutput

namespace rtbot::jit::emit {

// Stateful 1->1 windowed extremes via monotonic deque.
// Mirrors FE WIN_MIN/WIN_MAX bytecode opcodes (FusedScalarEval.h cases 40/41).
//
// State layout (2 + 2*W doubles at state_offset):
//   state[0]      — pos (number of messages seen)
//   state[1]      — deque size
//   state[2..W+1] — deque values
//   state[W+2..]  — deque positions
//
// Emits the current windowed minimum once pos >= W - 1.
StatefulOutput emit_win_min(IrEmissionContext& ec, std::size_t state_offset,
                            std::size_t W,
                            llvm::Value* t, llvm::Value* v);

// Emits the current windowed maximum once pos >= W - 1.
StatefulOutput emit_win_max(IrEmissionContext& ec, std::size_t state_offset,
                            std::size_t W,
                            llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_WINDOW_MIN_MAX_H
