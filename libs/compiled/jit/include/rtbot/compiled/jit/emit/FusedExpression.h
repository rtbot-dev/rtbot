#ifndef RTBOT_JIT_EMIT_FUSED_EXPRESSION_H
#define RTBOT_JIT_EMIT_FUSED_EXPRESSION_H

#include <cstddef>
#include <vector>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Result of walking one FusedExpression bytecode against synced inputs.
//   out_vs[0..num_outputs-1] hold the SSA double values for each END marker.
//   emit_flag is i1 — false when GATE / DIFF-warmup / windowed-warmup
//   suppresses this tick's emission.
struct FusedExprOutput {
  std::vector<llvm::Value*> out_vs;
  llvm::Value* emit_flag;  // i1
};

// Walk a public-form FE bytecode (interleaved opcodes + inline args; same
// shape callers pass to make_fused_expression) and emit IR that mirrors
// FusedScalarEval::evaluate_one bit-exactly.
//
// state_offset points at the FE's bytecode state region (i.e. AFTER the
// port queues). All stateful opcodes (CUMSUM/COUNT/MAX_AGG/MIN_AGG/STATE_LOAD,
// MA_UPDATE..IIR_UPDATE, DIFF, SIGN_CHANGE) GEP relative to this offset.
//
// inputs[i] is the SSA double for the i-th synced input port value.
//
// constants / coefficients are baked into IR as ConstantFP / global arrays.
FusedExprOutput emit_fused_expression(
    IrEmissionContext& ec,
    std::size_t state_offset,
    const std::vector<double>& bytecode,
    const std::vector<double>& constants,
    const std::vector<double>& coefficients,
    const std::vector<llvm::Value*>& inputs,
    std::size_t num_outputs);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_FUSED_EXPRESSION_H
