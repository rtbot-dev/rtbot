#include "fused_parity/reference_eval.h"

#include "rtbot/fuse/FusedScalarEval.h"

namespace rtbot::fused_parity {

// The "reference" scalar evaluator is the single production scalar hot path
// in libs/fuse. Tests compare batched / SIMD / dispatch variants against it
// via property-based fuzzing, and compare it against frozen SHA256 golden
// digests to catch regressions in the scalar path itself.
RefResult evaluate_scalar(
    const std::vector<double>& bytecode,
    const std::vector<double>& constants,
    const std::vector<std::vector<double>>& inputs_per_message,
    std::vector<double> state_init,
    std::size_t num_outputs) {
  RefResult result;
  result.state = std::move(state_init);
  result.outputs.resize(inputs_per_message.size() * num_outputs);

  const double* bc = bytecode.data();
  const std::size_t bc_size = bytecode.size();
  const double* consts = constants.data();
  double* out_ptr = result.outputs.data();

  for (std::size_t m = 0; m < inputs_per_message.size(); ++m) {
    rtbot::fuse::evaluate_one(
        bc, bc_size, consts,
        inputs_per_message[m].data(),
        result.state.data(),
        out_ptr + m * num_outputs,
        num_outputs);
  }

  return result;
}

}  // namespace rtbot::fused_parity
