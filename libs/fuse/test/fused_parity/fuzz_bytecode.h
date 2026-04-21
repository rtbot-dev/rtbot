#ifndef RTBOT_FUSED_PARITY_FUZZ_BYTECODE_H
#define RTBOT_FUSED_PARITY_FUZZ_BYTECODE_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rtbot::fused_parity {

struct FuzzProgram {
  std::size_t num_inputs;
  std::size_t num_outputs;
  std::vector<double> bytecode;
  std::vector<double> constants;
  std::vector<double> state_init;
};

// Generate a random, stack-balanced bytecode program. Pure arithmetic,
// unary math, comparisons, and boolean ops by default. Set include_transcendentals
// to allow LOG/EXP/SIN/COS/SQRT/POW. Set include_stateful to allow CUMSUM/COUNT/
// MAX_AGG/MIN_AGG/STATE_LOAD.
//
// Operand domains are sanitized at generation time:
//  - DIV uses a non-zero constant divisor.
//  - SQRT / LOG / LOG10 are preceded by an ABS (or an ABS + constant).
//  - POW bases wrapped in ABS.
//
// Seed is deterministic.
FuzzProgram generate_program(std::uint64_t seed,
                             std::size_t max_inputs = 8,
                             std::size_t max_outputs = 4,
                             bool include_transcendentals = false,
                             bool include_stateful = false);

// Generate deterministic synchronized inputs.
std::vector<std::vector<double>> generate_input_sequence(
    std::uint64_t seed, std::size_t num_inputs, std::size_t num_messages);

}  // namespace rtbot::fused_parity

#endif  // RTBOT_FUSED_PARITY_FUZZ_BYTECODE_H
