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

// Portable PRNG + distributions. Deliberately does not use
// std::uniform_real_distribution / std::normal_distribution because those
// are implementation-defined in the C++ standard — libc++ (macOS default)
// and libstdc++ (Linux default) produce different samples from the same
// seed, which makes any hash-based golden test architecture-specific.
//
// Arithmetic here avoids transcendentals (no sqrt/log/exp/sin/cos) so the
// input corpora are bit-exact across every IEEE 754 platform with the
// same compile flags.
struct PortableRng {
  std::uint64_t state;
  explicit PortableRng(std::uint64_t seed) : state(seed) {}
  // SplitMix64 — a well-known small PRNG with good statistical properties
  // whose output depends only on integer arithmetic.
  std::uint64_t next_u64() {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  // Map the high 53 bits to [0, 1). Constant 2^53 is exactly representable.
  double uniform_unit() {
    return static_cast<double>(next_u64() >> 11) /
           static_cast<double>(1ULL << 53);
  }
  double uniform(double lo, double hi) {
    return lo + uniform_unit() * (hi - lo);
  }
  // Irwin-Hall approximation of N(0, 1): sum of 12 uniforms minus 6. Not
  // as statistically pure as Box-Muller but avoids sqrt/log and is plenty
  // good for exercising a wide value range in tests.
  double normal(double mean, double sigma) {
    double sum = 0.0;
    for (int i = 0; i < 12; ++i) sum += uniform_unit();
    return mean + (sum - 6.0) * sigma;
  }
};

// Generate a synthetic input corpus exercising a wide value range.
// Deterministic by seed, portable across libc++ / libstdc++.
std::vector<std::vector<double>> synthetic_corpus(std::size_t num_inputs,
                                                    std::size_t num_messages,
                                                    std::uint64_t seed) {
  PortableRng rng(seed);
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
        msgs[t][p] = rng.uniform(-1e3, 1e3);
      } else {
        msgs[t][p] = rng.normal(0.0, 50.0);
      }
    }
  }
  return msgs;
}

// Random-walk generator used for the finance-like corpus.
std::vector<std::vector<double>> random_walk_corpus(std::size_t num_messages,
                                                      std::uint64_t seed) {
  PortableRng rng(seed);
  std::vector<std::vector<double>> msgs(num_messages,
                                         std::vector<double>(2));
  double price = 100.0;
  for (std::size_t t = 0; t < num_messages; ++t) {
    price += rng.normal(0.0, 1.0);
    msgs[t][0] = price;
    msgs[t][1] = rng.normal(0.0, 1.0);
  }
  return msgs;
}

// Hash a decimal serialization of each output rather than the raw bytes.
// The scalar evaluator calls libm (std::sin/std::cos/std::exp/std::pow), whose
// last-bit results differ across libm implementations (glibc vs macOS libc vs
// musl) even on the same ISA. Hashing raw bytes would make this test
// architecture- and stdlib-specific. Quantizing to 12 significant digits
// absorbs sub-ULP drift while still detecting any meaningful evaluator
// regression — the acceptable loss of precision is ~1e-12 relative.
std::string hash_outputs(const std::vector<double>& outputs) {
  Sha256Stream h;
  char buf[32];
  for (double v : outputs) {
    // %.12g produces up to 12 significant digits in the shortest
    // representation; for finite values this is platform-agnostic under
    // IEEE 754 rounding rules.
    int n = std::snprintf(buf, sizeof(buf), "%.12g", v);
    if (n > 0) {
      h.update(reinterpret_cast<const std::uint8_t*>(buf),
               static_cast<std::size_t>(n));
    }
    // Include a separator so adjacent values can't merge into the same
    // byte stream (e.g. "1|2" ≠ "12").
    const std::uint8_t sep = '|';
    h.update(&sep, 1);
  }
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
      "17c981b190b019db1c6e175990caf29a1679dc6242235116b8051e68ba47a3a3";

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
      "006eee34ddb274e213e98f165d8bd8dd97580ff4491e5de6eb757137f391a0af";

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
      "b59e239bf0d71ad1f001b2da6f09cbdde54a8b84642603e141f255a849233e31";

  auto inputs = random_walk_corpus(c.num_messages, c.seed);
  run_golden(c, inputs);
}
