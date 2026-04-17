#ifndef RTBOT_FUSED_PARITY_REFERENCE_EVAL_H
#define RTBOT_FUSED_PARITY_REFERENCE_EVAL_H

#include <cstddef>
#include <vector>

namespace rtbot::fused_parity {

// Canonical scalar RPN evaluator. Frozen at Phase 0. Later phases compare
// optimized variants against this. Bit-exact for non-transcendental opcodes;
// transcendentals use std:: libm identically to the current FusedExpression.
//
// bytecode/constants/state_init follow the same semantics as
// rtbot::FusedExpression. inputs_per_message[m][p] is the value on port p
// for message m. Outputs are flattened: outputs[m * num_outputs + k].
struct RefResult {
  std::vector<double> outputs;
  std::vector<double> state;  // final state after evaluation
};

RefResult evaluate_scalar(
    const std::vector<double>& bytecode,
    const std::vector<double>& constants,
    const std::vector<std::vector<double>>& inputs_per_message,
    std::vector<double> state_init,
    std::size_t num_outputs);

}  // namespace rtbot::fused_parity

#endif  // RTBOT_FUSED_PARITY_REFERENCE_EVAL_H
