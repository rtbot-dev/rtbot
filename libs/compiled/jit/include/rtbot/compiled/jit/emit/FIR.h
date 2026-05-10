#ifndef RTBOT_JIT_EMIT_FIR_H
#define RTBOT_JIT_EMIT_FIR_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // for StatefulOutput

namespace rtbot::jit::emit {

// Stateful FIR (Finite Impulse Response) filter.
// Mirrors FE FIR_UPDATE bytecode opcode (FusedScalarEval.h case 42).
//
// Coefficients are baked into the IR as a global constant array, so per-tick
// no extra parameter is needed. State layout (W + 2 doubles): ring + head + count.
//
// Emits the dot product of the ring against the coefficients once count >= W.
//
// t must be i64; v must be double.
StatefulOutput emit_fir(IrEmissionContext& ec, std::size_t state_offset,
                        std::size_t W,
                        const std::vector<double>& coefficients,  // size == W
                        llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_FIR_H
