#ifndef RTBOT_JIT_EMIT_IIR_H
#define RTBOT_JIT_EMIT_IIR_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // For StatefulOutput

namespace rtbot::jit::emit {

// Stateful IIR (Infinite Impulse Response) filter.
// Mirrors FE IIR_UPDATE bytecode opcode (FusedScalarEval.h case 43).
//
// State layout (4 + B_len + A_len doubles):
//   state[0] x_head, state[1] x_count, state[2] y_head, state[3] y_count
//   state[4..4+B_len-1] x_ring
//   state[4+B_len..4+B_len+A_len-1] y_ring
//
// Coefficients are baked as a global constant: first B_len b-coeffs, then A_len a-coeffs.
// Two Kahan-compensated summation loops (b * x_history) - (a * y_history).
// Emits y_n once x_ring is full.
StatefulOutput emit_iir(IrEmissionContext& ec, std::size_t state_offset,
                        std::size_t B_len, std::size_t A_len,
                        const std::vector<double>& coefficients,  // size = B_len + A_len
                        llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_IIR_H
