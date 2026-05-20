#ifndef RTBOT_JIT_EMIT_STATE_LOAD_H
#define RTBOT_JIT_EMIT_STATE_LOAD_H

#include <cstddef>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Read state[state_offset] and return as a double. Used to share values
// across output expressions in fused programs.
//
// Mirrors FE STATE_LOAD bytecode opcode (FusedScalarEval.h case 25).
//
// `state_offset` is the absolute offset (in doubles) into the segment's
// state buffer. The slot is presumably written by a prior opcode in the
// same program (CUMSUM, COUNT, MA_UPDATE result-slot, etc.); the JIT
// compiler is responsible for placing the right offset based on the
// JSON's StateLoad arg.
llvm::Value* emit_state_load(IrEmissionContext& ec, std::size_t state_offset);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_STATE_LOAD_H
