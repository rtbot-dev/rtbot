#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>

#include "fused_parity/fuzz_bytecode.h"
#include "fused_parity/live_drivers.h"
#include "fused_parity/reference_eval.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/fuse/FusedExpressionVector.h"

using namespace rtbot;
using namespace rtbot::fused_parity;

namespace {

inline std::uint64_t double_bits(double v) {
  std::uint64_t bits;
  std::memcpy(&bits, &v, sizeof(double));
  return bits;
}

void run_trial(const FuzzProgram& prog, std::size_t seq_len,
               std::uint64_t input_seed) {
  auto inputs = generate_input_sequence(input_seed, prog.num_inputs, seq_len);
  auto ref = evaluate_scalar(prog.bytecode, prog.constants, inputs,
                             prog.state_init, prog.num_outputs);
  REQUIRE(ref.outputs.size() == seq_len * prog.num_outputs);

  auto fe_flat = drive_fused_expression(prog.bytecode, prog.constants, inputs,
                                          prog.state_init, prog.num_inputs,
                                          prog.num_outputs);
  REQUIRE(fe_flat.size() == ref.outputs.size());
  for (std::size_t i = 0; i < fe_flat.size(); ++i) {
    INFO("i=" << i << " (FE)");
    REQUIRE(double_bits(fe_flat[i]) == double_bits(ref.outputs[i]));
  }

  auto fev_flat = drive_fused_expression_vector(prog.bytecode, prog.constants,
                                                  inputs, prog.state_init,
                                                  prog.num_outputs);
  REQUIRE(fev_flat.size() == ref.outputs.size());
  for (std::size_t i = 0; i < fev_flat.size(); ++i) {
    INFO("i=" << i << " (FEV)");
    REQUIRE(double_bits(fev_flat[i]) == double_bits(ref.outputs[i]));
  }
}

}  // namespace

SCENARIO(
    "Random stateless bytecode programs match scalar reference bit-exactly",
    "[fuzz][bit_exact]") {
  const int kTrials = 2000;
  const std::size_t kSeqLen = 16;

  for (int i = 0; i < kTrials; ++i) {
    std::uint64_t seed = 1337ULL + static_cast<std::uint64_t>(i);
    auto prog = generate_program(seed, /*max_inputs=*/6, /*max_outputs=*/3,
                                  /*include_transcendentals=*/false,
                                  /*include_stateful=*/false);
    std::ostringstream tag;
    tag << "seed=" << seed << " ni=" << prog.num_inputs
        << " no=" << prog.num_outputs << " bc_len=" << prog.bytecode.size();
    INFO(tag.str());
    run_trial(prog, kSeqLen, seed ^ 0xABCD);
  }
}

SCENARIO("Random stateful bytecode programs match scalar reference bit-exactly",
         "[fuzz][stateful]") {
  const int kTrials = 500;
  const std::size_t kSeqLen = 64;

  for (int i = 0; i < kTrials; ++i) {
    std::uint64_t seed = 9001ULL + static_cast<std::uint64_t>(i);
    auto prog = generate_program(seed, /*max_inputs=*/4, /*max_outputs=*/2,
                                  /*include_transcendentals=*/false,
                                  /*include_stateful=*/true);
    std::ostringstream tag;
    tag << "seed=" << seed << " ni=" << prog.num_inputs
        << " no=" << prog.num_outputs << " bc_len=" << prog.bytecode.size()
        << " state=" << prog.state_init.size();
    INFO(tag.str());
    run_trial(prog, kSeqLen, seed ^ 0xFEEDFACEULL);
  }
}

SCENARIO(
    "Random bytecode programs with transcendentals match scalar reference "
    "bit-exactly under scalar libm",
    "[fuzz][transcendental]") {
  const int kTrials = 500;
  const std::size_t kSeqLen = 8;

  for (int i = 0; i < kTrials; ++i) {
    std::uint64_t seed = 31337ULL + static_cast<std::uint64_t>(i);
    auto prog = generate_program(seed, /*max_inputs=*/4, /*max_outputs=*/2,
                                  /*include_transcendentals=*/true,
                                  /*include_stateful=*/false);
    std::ostringstream tag;
    tag << "seed=" << seed << " ni=" << prog.num_inputs
        << " no=" << prog.num_outputs << " bc_len=" << prog.bytecode.size();
    INFO(tag.str());
    run_trial(prog, kSeqLen, seed ^ 0xCAFED00DULL);
  }
}
