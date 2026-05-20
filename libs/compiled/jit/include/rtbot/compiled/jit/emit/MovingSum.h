#ifndef RTBOT_JIT_EMIT_MOVING_SUM_H
#define RTBOT_JIT_EMIT_MOVING_SUM_H

#include <cstddef>
#include <cstdint>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // For StatefulOutput

namespace rtbot::jit::emit {

// Stateful 1->1 emitter for MovingSum<W>. Identical to MovingAverage except
// the emitted value is the windowed sum (no final divide).
//
// State layout (W + 3 doubles): ring + Kahan sum + Kahan comp + count.
// Mirrors FE MSUM_UPDATE bytecode opcode (FusedScalarEval.h case 36).
StatefulOutput emit_moving_sum(IrEmissionContext& ec, std::size_t state_offset,
                                std::size_t W,
                                llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_MOVING_SUM_H
