#include "fused_parity/reference_eval.h"

#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedScalarEval.h"

namespace rtbot::fused_parity {

// The "reference" scalar evaluator is the single production scalar hot path
// in libs/fuse. Tests compare batched / SIMD / dispatch variants against it
// via property-based fuzzing, and compare it against frozen SHA256 golden
// digests to catch regressions in the scalar path itself.
//
// Windowed opcodes (Phase 3, e.g. MA_UPDATE) signal warmup suppression via
// evaluate_one's return value. We only append outputs for messages where the
// evaluator says "emit". Programs without windowed opcodes always emit.
RefResult evaluate_scalar(
    const std::vector<double>& bytecode,
    const std::vector<double>& constants,
    const std::vector<std::vector<double>>& inputs_per_message,
    std::vector<double> state_init,
    std::size_t num_outputs) {
  RefResult result;
  result.state = std::move(state_init);
  result.outputs.reserve(inputs_per_message.size() * num_outputs);

  const auto packed = rtbot::fuse::encode_legacy(bytecode);
  const rtbot::fuse::Instruction* ins = packed.data();
  const std::size_t ins_size = packed.size();
  const double* consts = constants.data();

  std::vector<double> scratch(num_outputs);
  for (std::size_t m = 0; m < inputs_per_message.size(); ++m) {
    const bool emit = rtbot::fuse::evaluate_one(
        ins, ins_size, consts,
        /*aux_args=*/nullptr,
        /*coefficients=*/nullptr,
        inputs_per_message[m].data(),
        result.state.data(),
        scratch.data(),
        num_outputs);
    if (emit) {
      result.outputs.insert(result.outputs.end(), scratch.begin(),
                             scratch.end());
    }
  }

  return result;
}

}  // namespace rtbot::fused_parity
