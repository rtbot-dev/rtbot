// Golden regression: feeds a deterministic input corpus through the frozen
// scalar reference evaluator, hashes the output stream, and compares the
// SHA256 to a committed golden digest. Any change to the reference or the
// corpus will invalidate the digest — the test output then prints the new
// digest which must be reviewed and committed.

#include <catch2/catch.hpp>

#include <cmath>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "fused_parity/fuzz_bytecode.h"
#include "fused_parity/live_drivers.h"
#include "fused_parity/reference_eval.h"
#include "fused_parity/sha256_stream.h"
#include "rtbot/fuse/FusedExpression.h"

using namespace rtbot;
using namespace rtbot::fused_op;
using namespace rtbot::fused_parity;

namespace {

// Generate a synthetic input corpus exercising a wide value range.
// Deterministic by seed.
std::vector<std::vector<double>> synthetic_corpus(std::size_t num_inputs,
                                                    std::size_t num_messages,
                                                    std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> uniform(-1e3, 1e3);
  std::normal_distribution<double> normal(0.0, 50.0);
  std::vector<std::vector<double>> msgs(num_messages,
                                         std::vector<double>(num_inputs));
  for (std::size_t t = 0; t < num_messages; ++t) {
    for (std::size_t p = 0; p < num_inputs; ++p) {
      // Alternate distributions across ports; inject corner cases periodically.
      if (t % 97 == 0 && p == 0) {
        msgs[t][p] = 0.0;
      } else if (t % 199 == 0 && p == 0) {
        msgs[t][p] = -1.0;
      } else if (p % 2 == 0) {
        msgs[t][p] = uniform(rng);
      } else {
        msgs[t][p] = normal(rng);
      }
    }
  }
  return msgs;
}

// Random-walk generator used for the finance-like corpus.
std::vector<std::vector<double>> random_walk_corpus(std::size_t num_messages,
                                                      std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::normal_distribution<double> step(0.0, 1.0);
  std::vector<std::vector<double>> msgs(num_messages,
                                         std::vector<double>(2));
  double price = 100.0;
  for (std::size_t t = 0; t < num_messages; ++t) {
    price += step(rng);
    msgs[t][0] = price;
    msgs[t][1] = step(rng);  // secondary noise channel
  }
  return msgs;
}

std::string hash_outputs(const std::vector<double>& outputs) {
  Sha256Stream h;
  for (double v : outputs) h.update(v);
  return h.finalize();
}

struct GoldenCase {
  const char* name;
  std::vector<double> bytecode;
  std::vector<double> constants;
  std::vector<double> state_init;
  std::size_t num_inputs;
  std::size_t num_outputs;
  std::size_t num_messages;
  std::uint64_t seed;
  const char* expected_digest;  // seed from first-run failure output
};

void run_golden(const GoldenCase& c,
                const std::vector<std::vector<double>>& inputs) {
  REQUIRE(inputs.size() == c.num_messages);
  REQUIRE(!inputs.empty());
  REQUIRE(inputs[0].size() == c.num_inputs);

  // 1. Reference scalar evaluator → golden digest.
  auto ref = evaluate_scalar(c.bytecode, c.constants, inputs, c.state_init,
                             c.num_outputs);
  REQUIRE(ref.outputs.size() == c.num_messages * c.num_outputs);
  std::string ref_digest = hash_outputs(ref.outputs);
  INFO("case=" << c.name << " ref_digest=" << ref_digest);
  REQUIRE(ref_digest == std::string(c.expected_digest));

  // 2. Live FusedExpression (scalar ports) must produce the same stream.
  auto fe_outputs = drive_fused_expression(c.bytecode, c.constants, inputs,
                                             c.state_init, c.num_inputs,
                                             c.num_outputs);
  REQUIRE(fe_outputs.size() == ref.outputs.size());
  std::string fe_digest = hash_outputs(fe_outputs);
  INFO("case=" << c.name << " fe_digest=" << fe_digest);
  REQUIRE(fe_digest == ref_digest);

  // 3. Live FusedExpressionVector (single vector port) must match too.
  auto fev_outputs = drive_fused_expression_vector(c.bytecode, c.constants,
                                                     inputs, c.state_init,
                                                     c.num_outputs);
  REQUIRE(fev_outputs.size() == ref.outputs.size());
  std::string fev_digest = hash_outputs(fev_outputs);
  INFO("case=" << c.name << " fev_digest=" << fev_digest);
  REQUIRE(fev_digest == ref_digest);
}

}  // namespace

SCENARIO("Synthetic corpus produces the frozen golden hash",
         "[golden][synthetic]") {
  // Program covers: arithmetic, abs, sqrt (via abs), pow, negation, a
  // comparison, a boolean, and stateful CUMSUM — a representative cross-section
  // of non-transcendental + a couple of transcendentals.
  //
  // Outputs:
  //   0: INPUT0 + INPUT1
  //   1: INPUT2 * INPUT3
  //   2: sqrt(abs(INPUT0))
  //   3: NEG(INPUT1 - INPUT2)
  //   4: INPUT3 ^ 2
  //   5: (INPUT0 > INPUT1) OR (INPUT2 < INPUT3)
  //   6: CUMSUM(INPUT0) — stateful
  GoldenCase c;
  c.name = "synthetic";
  c.bytecode = {INPUT, 0, INPUT, 1, ADD, END,
                INPUT, 2, INPUT, 3, MUL, END,
                INPUT, 0, ABS, SQRT, END,
                INPUT, 1, INPUT, 2, SUB, NEG, END,
                INPUT, 3, CONST, 0, POW, END,
                INPUT, 0, INPUT, 1, GT, INPUT, 2, INPUT, 3, LT, OR, END,
                INPUT, 0, CUMSUM, 0, END};
  c.constants = {2.0};
  c.state_init = {0.0, 0.0};
  c.num_inputs = 4;
  c.num_outputs = 7;
  c.num_messages = 10000;
  c.seed = 0xF05EDBEEFULL;
  c.expected_digest =
      "152722b1b73374386cd099157bc853df77944cd82868b8ff953405ffe7eb1b3d";

  auto inputs = synthetic_corpus(c.num_inputs, c.num_messages, c.seed);
  run_golden(c, inputs);
}

SCENARIO("IMS-like projection produces the frozen golden hash",
         "[golden][ims]") {
  // Simulates a typical IMS bearing analytics projection: device_id
  // passthrough, channel_id passthrough, rms = sqrt(ex2), std =
  // sqrt(ex2 - mean^2), crest = max / rms (approximated with abs/sign math).
  //
  // Input ports: 0=device_id, 1=channel_id, 2=ex2 (expected squared value),
  // 3=mean_value.
  //
  // Outputs:
  //   0: INPUT0 (passthrough)
  //   1: INPUT1 (passthrough)
  //   2: sqrt(abs(INPUT2))                — rms
  //   3: sqrt(abs(INPUT2 - INPUT3*INPUT3)) — std
  //   4: INPUT2 / (abs(INPUT3) + CONST 1)  — rough crest-like ratio
  GoldenCase c;
  c.name = "ims_like";
  c.bytecode = {INPUT, 0, END,
                INPUT, 1, END,
                INPUT, 2, ABS, SQRT, END,
                INPUT, 2, INPUT, 3, INPUT, 3, MUL, SUB, ABS, SQRT, END,
                INPUT, 2, INPUT, 3, ABS, CONST, 0, ADD, DIV, END};
  c.constants = {1.0e-9};
  c.state_init = {};
  c.num_inputs = 4;
  c.num_outputs = 5;
  c.num_messages = 10000;
  c.seed = 0x115DA7AULL;
  c.expected_digest =
      "b8460d323dbca8d74419ca8a7300ae1792a57499bc95d003d909eea18d680e7c";

  auto inputs = synthetic_corpus(c.num_inputs, c.num_messages, c.seed);
  run_golden(c, inputs);
}

SCENARIO("Finance-like projection over random-walk produces the frozen hash",
         "[golden][finance]") {
  // Finance preset flavor: cumulative sum (for running PnL) + count-based
  // running mean (AVG decomposed as CUMSUM / COUNT). Outputs:
  //   0: CUMSUM(INPUT0)             — running sum of price
  //   1: COUNT                      — tick count (state_off=2)
  //   2: STATE_LOAD(sum) / STATE_LOAD(count) — running average
  //   3: INPUT0 - STATE_LOAD(sum)/STATE_LOAD(count) — deviation from running mean
  GoldenCase c;
  c.name = "finance_like";
  c.bytecode = {INPUT, 0, CUMSUM, 0, END,
                COUNT, 2, END,
                STATE_LOAD, 0, STATE_LOAD, 2, DIV, END,
                INPUT, 0, STATE_LOAD, 0, STATE_LOAD, 2, DIV, SUB, END};
  c.constants = {};
  c.state_init = {0.0, 0.0, 0.0};  // [sum, kahan, count]
  c.num_inputs = 2;
  c.num_outputs = 4;
  c.num_messages = 20000;
  c.seed = 0xF11A4CE1ULL;
  c.expected_digest =
      "21c46dea97ac619e56a392ca5c553b15482db56b3d328f06e2a92e92642ed105";

  auto inputs = random_walk_corpus(c.num_messages, c.seed);
  run_golden(c, inputs);
}
