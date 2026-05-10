// test_state_load_json_e2e.cpp
//
// End-to-end test for the StateLoad JSON integration.
//
// Pipeline:
//   input -> cumsum -> out (port i1)
//   StateLoad(source=cumsum) -> scale(*2) -> out (port i2)
//
// StateLoad has no explicit data connection from cumsum; it references it via
// the `source` field. The SegmentPartitioner adds an implicit ordering edge so
// cumsum always executes before the StateLoad node in topological order.
//
// Expected invariants for every emitted tick:
//   out_v[0] == cumulative_sum_so_far           (direct cumsum output)
//   out_v[1] == cumulative_sum_so_far * 2.0     (StateLoad -> Scale)
//
// CumSum uses Kahan compensation internally; both output values should be
// bit-identical for the same input sequence driven through the same state.

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/compiled/jit/JitCompiler.h"

namespace {

// JSON program: CumSum + StateLoad-Scale dual output.
//
// Operator ordering note: StateLoad appears AFTER CumSum in the operators
// array. Even if it appeared before, the implicit ordering edge added by
// SegmentPartitioner ensures correctness. The ordering here makes the test
// self-documenting and robust.
const char* kStateLoadJson = R"({
  "title": "Double Cumsum Test",
  "apiVersion": "v1",
  "entryOperator": "input",
  "output": { "out": ["o1", "o2"] },
  "operators": [
    { "id": "input",  "type": "Input",         "portTypes": ["number"] },
    { "id": "cumsum", "type": "CumulativeSum" },
    { "id": "loadcs", "type": "StateLoad",     "source": "cumsum" },
    { "id": "scale",  "type": "Scale",         "value": 2 },
    { "id": "out",    "type": "Output",        "portTypes": ["number", "number"] }
  ],
  "connections": [
    { "from": "input",  "to": "cumsum", "fromPort": "o1", "toPort": "i1" },
    { "from": "cumsum", "to": "out",    "fromPort": "o1", "toPort": "i1" },
    { "from": "loadcs", "to": "scale",  "fromPort": "o1", "toPort": "i1" },
    { "from": "scale",  "to": "out",    "fromPort": "o1", "toPort": "i2" }
  ]
})";

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

}  // namespace

SCENARIO("JIT StateLoad JSON e2e: StateLoad reads cumsum state and drives a second output",
         "[state_load][json][e2e]") {
  // Drive 100 random inputs.
  constexpr std::size_t N = 100;
  std::mt19937_64 rng(0xDEAD'BEEF'1234ULL);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);

  std::vector<double> inputs(N);
  for (auto& x : inputs) x = dist(rng);

  // Compile via JitCompiler.
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(kStateLoadJson);
  REQUIRE(prog != nullptr);

  // Drive all inputs.
  for (std::size_t i = 0; i < N; ++i) {
    prog->receive(static_cast<std::int64_t>(i + 1), inputs[i]);
  }

  const auto& emits = prog->collect_outputs();

  // CumSum always emits, so we expect N outputs.
  REQUIRE(emits.size() == N);

  // Reference: compute expected cumulative sums via Kahan summation to match
  // the emitter's internal implementation exactly.
  double kahan_sum  = 0.0;
  double kahan_comp = 0.0;

  std::size_t check_count = 0;
  for (std::size_t i = 0; i < N; ++i) {
    // Kahan update (mirrors emit_cumsum).
    double y = inputs[i] - kahan_comp;
    double t = kahan_sum + y;
    kahan_comp = (t - kahan_sum) - y;
    kahan_sum  = t;

    const auto& r = emits[i];

    INFO("tick=" << (i + 1)
         << " expected_sum=" << kahan_sum
         << " out_v[0]=" << r.values[0]
         << " out_v[1]=" << r.values[1]);

    // out_v[0] == cumulative sum
    REQUIRE(dbits(r.values[0]) == dbits(kahan_sum));

    // out_v[1] == cumulative sum * 2.0 (via StateLoad -> Scale)
    double expected_scaled = kahan_sum * 2.0;
    REQUIRE(dbits(r.values[1]) == dbits(expected_scaled));

    ++check_count;
  }

  // Sanity: all N ticks were checked.
  REQUIRE(check_count == N);
}
