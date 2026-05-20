// test_framework_parity_audit2.cpp
//
// Second-wave audit-driven parity SCENARIOs for the JIT, targeting shapes the
// first audit (test_framework_parity_audit.cpp) did not exercise. Each SCENARIO
// drives the same Program JSON through both JIT and interpreter via
// rtbot::test::run_jit_vs_interpreter_parity and REQUIREs bit-exact emission
// equality.
//
// Scope notes
// -----------
// A handful of operator families listed in the audit dispatch are not part of
// the public Program JSON schema (FusedExpression, FusedExpressionVector,
// BurstAggregate, TopK, WindowMinMax, MinTracker, MaxTracker). They are
// instantiable only through JitCompiler::compile directly, which means they
// cannot be driven through Program(json) and the parity helper. Their JIT-vs-
// reference coverage already lives in the per-op parity files
// (test_fused_expression_parity.cpp, test_topk_parity.cpp,
// test_burst_aggregate_parity.cpp, test_window_min_max_emit.cpp). This audit
// fills the *Program-level* gaps where the JIT path and the framework path
// could diverge end-to-end.
//
// Coverage:
//   Tier 1: KeyedPipeline at high cardinality (50+ unique keys, both
//           simple-key and computed-key modes), vector chains
//           (VectorCompose -> VectorExtract / VectorProject), Demultiplexer
//           with multiple controls firing in the same tick.
//   Tier 2: MovingKeyCount with windowed eviction stress.
//   Tier 3: NaN / +/-Inf inputs, long-run drift (10k+ ticks), single-tick,
//           timestamps near INT64 boundaries.
//   Mid-tier: FIR with realistic coefficients, Linear N=5 inputs, ReduceJoin
//             arithmetic + boolean variants (parameterised),
//             ArithmeticScalar transcendental family,
//             CompareScalar and CompareSync variants (with BooleanToNumber
//             bridge so the Output port type matches).
//
// Deterministic input: all random streams use fixed seeds (mt19937_64). Each
// SCENARIO documents its seed inline.

#include <catch2/catch.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "parity_helper.h"
#include "rtbot/Collector.h"
#include "rtbot/Message.h"
#include "rtbot/Program.h"
#include "rtbot/compiled/jit/JitCompiler.h"
#include "rtbot/std/FilterScalar.h"
#include "rtbot/std/MinMaxTracker.h"

namespace {

// ---------------------------------------------------------------------------
// JIT-engagement probe. Each SCENARIO calls this with the program JSON before
// invoking the parity helper, ensuring the test exercises JIT-vs-interpreter
// rather than interpreter-vs-interpreter.
// ---------------------------------------------------------------------------
void require_jit_engaged(const std::string& json) {
  rtbot::Program::set_force_interpreter_for_testing(false);
  rtbot::Program prog(json);
  REQUIRE(prog.using_jit());
}

// ---------------------------------------------------------------------------
// Common drive callbacks.
// ---------------------------------------------------------------------------
auto send_scalar() {
  return [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
    p.send(tick.t, tick.values[0]);
  };
}

auto send_vector_n(std::size_t n) {
  return [n](rtbot::Program& p, const rtbot::test::TickInput& tick) {
    p.send_vector(tick.t, tick.values.data(), n);
  };
}

// ---------------------------------------------------------------------------
// Deterministic input generators.
// ---------------------------------------------------------------------------
std::vector<rtbot::test::TickInput> sine(int count, double amp, double freq,
                                          double phase = 0.0) {
  std::vector<rtbot::test::TickInput> out;
  out.reserve(count);
  for (int i = 1; i <= count; ++i) {
    out.push_back({static_cast<std::int64_t>(i),
                   {amp * std::sin(freq * static_cast<double>(i) + phase)}});
  }
  return out;
}

std::vector<rtbot::test::TickInput> mt_random(int count, std::uint64_t seed,
                                               double lo = -10.0,
                                               double hi = 10.0) {
  std::vector<rtbot::test::TickInput> out;
  out.reserve(count);
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(lo, hi);
  for (int i = 1; i <= count; ++i) {
    out.push_back({static_cast<std::int64_t>(i), {dist(rng)}});
  }
  return out;
}

// ---------------------------------------------------------------------------
// JSON helpers.
// ---------------------------------------------------------------------------
const char* kKeyedPipelineSimpleJson = R"({
  "title":"t","apiVersion":"v1","entryOperator":"in",
  "output":{"out":["o1"]},
  "operators":[
    {"id":"in","type":"Input","portTypes":["vector_number"],"portWidths":[2]},
    {"id":"kp","type":"KeyedPipeline","key_index":0,
      "prototype":{
        "operators":[
          {"id":"pin","type":"Input","portTypes":["vector_number"]},
          {"id":"vex","type":"VectorExtract","index":1},
          {"id":"cs","type":"CumulativeSum"},
          {"id":"pout","type":"Output","portTypes":["number"]}
        ],
        "connections":[
          {"from":"pin","to":"vex","fromPort":"o1","toPort":"i1"},
          {"from":"vex","to":"cs","fromPort":"o1","toPort":"i1"},
          {"from":"cs","to":"pout","fromPort":"o1","toPort":"i1"}
        ],
        "entry":{"operator":"pin"},
        "output":{"operator":"pout"}
      }
    },
    {"id":"out","type":"Output","portTypes":["vector_number"]}
  ],
  "connections":[
    {"from":"in","to":"kp","fromPort":"o1","toPort":"i1"},
    {"from":"kp","to":"out","fromPort":"o1","toPort":"i1"}
  ]
})";

// Computed-key KeyedPipeline forwards the inner emit type. The inner Count
// emits NumberData; KP outputs NumberData in computed-key mode.
const char* kKeyedPipelineComputedJson = R"({
  "title":"t","apiVersion":"v1","entryOperator":"in",
  "output":{"out":["o1"]},
  "operators":[
    {"id":"in","type":"Input","portTypes":["vector_number"],"portWidths":[3]},
    {"id":"kp","type":"KeyedPipeline","keyColumnIndices":[0,1],
      "prototype":{
        "operators":[
          {"id":"pin","type":"Input","portTypes":["vector_number"]},
          {"id":"vex","type":"VectorExtract","index":2},
          {"id":"cnt","type":"CountNumber"},
          {"id":"pout","type":"Output","portTypes":["number"]}
        ],
        "connections":[
          {"from":"pin","to":"vex","fromPort":"o1","toPort":"i1"},
          {"from":"vex","to":"cnt","fromPort":"o1","toPort":"i1"},
          {"from":"cnt","to":"pout","fromPort":"o1","toPort":"i1"}
        ],
        "entry":{"operator":"pin"},
        "output":{"operator":"pout"}
      }
    },
    {"id":"out","type":"Output","portTypes":["vector_number"]}
  ],
  "connections":[
    {"from":"in","to":"kp","fromPort":"o1","toPort":"i1"},
    {"from":"kp","to":"out","fromPort":"o1","toPort":"i1"}
  ]
})";

}  // namespace

// ===========================================================================
// Tier 1: KeyedPipeline at high cardinality.
// ===========================================================================

SCENARIO("audit2 parity: KeyedPipeline simple-key 50 unique keys interleaved",
         "[program_audit2][parity][keyed_pipeline]") {
  // 50 unique keys, 600 ticks (12 visits each on average). Seed: 0xBEEF.
  std::vector<rtbot::test::TickInput> inputs;
  inputs.reserve(600);
  std::mt19937_64 rng(0xBEEFULL);
  std::uniform_real_distribution<double> dist(-25.0, 25.0);
  for (std::int64_t t = 1; t <= 600; ++t) {
    const double key = static_cast<double>((t - 1) % 50);
    inputs.push_back({t, {key, dist(rng)}});
  }
  require_jit_engaged(kKeyedPipelineSimpleJson);

  GIVEN("a KeyedPipeline cycling through 50 keys with random per-tick values") {
    rtbot::test::run_jit_vs_interpreter_parity(kKeyedPipelineSimpleJson,
                                                inputs, send_vector_n(2));
  }
}

SCENARIO("audit2 parity: KeyedPipeline simple-key revisits after long gaps",
         "[program_audit2][parity][keyed_pipeline]") {
  // Pattern: key 7 for 5 ticks, then keys 0..40 each once (40 distinct keys
  // touched), then key 7 again. Confirms per-key state persists across many
  // intervening other-key visits. Seed: 0xC0FFEE.
  std::vector<rtbot::test::TickInput> inputs;
  std::int64_t t = 1;
  std::mt19937_64 rng(0xC0FFEEULL);
  std::uniform_real_distribution<double> dist(0.0, 10.0);
  for (int i = 0; i < 5; ++i) inputs.push_back({t++, {7.0, dist(rng)}});
  for (int k = 0; k < 41; ++k) {
    if (k == 7) continue;
    inputs.push_back({t++, {static_cast<double>(k), dist(rng)}});
  }
  for (int i = 0; i < 5; ++i) inputs.push_back({t++, {7.0, dist(rng)}});
  for (int k = 0; k < 41; ++k) {
    if (k == 7) continue;
    inputs.push_back({t++, {static_cast<double>(k), dist(rng)}});
  }
  for (int i = 0; i < 5; ++i) inputs.push_back({t++, {7.0, dist(rng)}});
  require_jit_engaged(kKeyedPipelineSimpleJson);

  GIVEN("a single key revisited after many intervening other-key visits") {
    rtbot::test::run_jit_vs_interpreter_parity(kKeyedPipelineSimpleJson,
                                                inputs, send_vector_n(2));
  }
}

SCENARIO("audit2 parity: KeyedPipeline computed-key polynomial-hash 60 keys",
         "[program_audit2][parity][keyed_pipeline]") {
  // 4 distinct a-values x 15 distinct b-values = 60 logical keys. Seed: 0xD00D.
  std::vector<rtbot::test::TickInput> inputs;
  inputs.reserve(900);
  std::mt19937_64 rng(0xD00DULL);
  std::uniform_real_distribution<double> dist(-10.0, 10.0);
  for (std::int64_t t = 1; t <= 900; ++t) {
    const double a = static_cast<double>((t - 1) % 4);
    const double b = static_cast<double>((t - 1) % 15);
    inputs.push_back({t, {a, b, dist(rng)}});
  }
  require_jit_engaged(kKeyedPipelineComputedJson);

  GIVEN("a computed-key KeyedPipeline at 60 logical keys") {
    rtbot::test::run_jit_vs_interpreter_parity(kKeyedPipelineComputedJson,
                                                inputs, send_vector_n(3));
  }
}

// ===========================================================================
// Tier 1: Vector chains. VectorCompose into VectorExtract / VectorProject.
// ===========================================================================

SCENARIO("audit2 parity: VectorCompose(N=5) -> VectorExtract(idx=3)",
         "[program_audit2][parity][vector]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"b0","type":"Identity"},
      {"id":"b1","type":"Add","value":1.0},
      {"id":"b2","type":"Add","value":2.0},
      {"id":"b3","type":"Add","value":3.0},
      {"id":"b4","type":"Add","value":4.0},
      {"id":"vc","type":"VectorCompose","numPorts":5},
      {"id":"ve","type":"VectorExtract","index":3},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"b0","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b1","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b2","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b3","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b4","fromPort":"o1","toPort":"i1"},
      {"from":"b0","to":"vc","fromPort":"o1","toPort":"i1"},
      {"from":"b1","to":"vc","fromPort":"o1","toPort":"i2"},
      {"from":"b2","to":"vc","fromPort":"o1","toPort":"i3"},
      {"from":"b3","to":"vc","fromPort":"o1","toPort":"i4"},
      {"from":"b4","to":"vc","fromPort":"o1","toPort":"i5"},
      {"from":"vc","to":"ve","fromPort":"o1","toPort":"i1"},
      {"from":"ve","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  auto inputs = mt_random(80, 0x12345ULL);

  GIVEN("a width-5 VectorCompose with VectorExtract picking lane 3") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

SCENARIO("audit2 parity: VectorCompose(N=6) -> VectorProject(indices=[0,2,4])",
         "[program_audit2][parity][vector]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"b0","type":"Identity"},
      {"id":"b1","type":"Add","value":10.0},
      {"id":"b2","type":"Scale","value":2.0},
      {"id":"b3","type":"Add","value":100.0},
      {"id":"b4","type":"Scale","value":-1.0},
      {"id":"b5","type":"Add","value":-7.5},
      {"id":"vc","type":"VectorCompose","numPorts":6},
      {"id":"vp","type":"VectorProject","indices":[0,2,4]},
      {"id":"out","type":"Output","portTypes":["vector_number"],"portWidths":[3]}
    ],
    "connections":[
      {"from":"in","to":"b0","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b1","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b2","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b3","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b4","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b5","fromPort":"o1","toPort":"i1"},
      {"from":"b0","to":"vc","fromPort":"o1","toPort":"i1"},
      {"from":"b1","to":"vc","fromPort":"o1","toPort":"i2"},
      {"from":"b2","to":"vc","fromPort":"o1","toPort":"i3"},
      {"from":"b3","to":"vc","fromPort":"o1","toPort":"i4"},
      {"from":"b4","to":"vc","fromPort":"o1","toPort":"i5"},
      {"from":"b5","to":"vc","fromPort":"o1","toPort":"i6"},
      {"from":"vc","to":"vp","fromPort":"o1","toPort":"i1"},
      {"from":"vp","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  auto inputs = mt_random(80, 0x23456ULL);

  GIVEN("VectorCompose(6) projecting down to lanes [0,2,4]") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

SCENARIO("audit2 parity: VectorCompose(N=4) -> VectorProject(indices=[1,3])",
         "[program_audit2][parity][vector]") {
  // Smaller projection picking only the odd lanes.
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"b0","type":"Identity"},
      {"id":"b1","type":"Scale","value":2.0},
      {"id":"b2","type":"Add","value":5.0},
      {"id":"b3","type":"Scale","value":-1.0},
      {"id":"vc","type":"VectorCompose","numPorts":4},
      {"id":"vp","type":"VectorProject","indices":[1,3]},
      {"id":"out","type":"Output","portTypes":["vector_number"],"portWidths":[2]}
    ],
    "connections":[
      {"from":"in","to":"b0","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b1","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b2","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b3","fromPort":"o1","toPort":"i1"},
      {"from":"b0","to":"vc","fromPort":"o1","toPort":"i1"},
      {"from":"b1","to":"vc","fromPort":"o1","toPort":"i2"},
      {"from":"b2","to":"vc","fromPort":"o1","toPort":"i3"},
      {"from":"b3","to":"vc","fromPort":"o1","toPort":"i4"},
      {"from":"vc","to":"vp","fromPort":"o1","toPort":"i1"},
      {"from":"vp","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  auto inputs = mt_random(80, 0x34567ULL);

  GIVEN("VectorCompose(4) projecting lanes [1,3]") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

// ===========================================================================
// Tier 1: Demultiplexer with multiple controls firing in same tick.
// ===========================================================================

// !mayfail JIT divergence: when multiple Demux controls fire on the same tick
// the JIT emits ~2.6x more emissions than the interpreter (370 vs 140 over 200
// ticks at seed 0x45678). Same shape as the asymmetric multi-port Output bug
// pinned by the first audit's CountDispatchTwoPort SCENARIO. Tagged so the
// divergence is locked under audit until the in-flight Output handler fix
// lands.
SCENARIO("audit2 parity: Demultiplexer N=3 with overlapping controls",
         "[program_audit2][parity][demux]") {
  // Controls: c1 = (v > 0), c2 = (v >= 0), c3 = (v >= 1). When v=2, all three
  // emit; when v=0, only c2; when v<0, none.
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1","o2","o3"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"data","type":"Identity"},
      {"id":"c1","type":"CompareGT","value":0.0},
      {"id":"c2","type":"CompareGTE","value":0.0},
      {"id":"c3","type":"CompareGTE","value":1.0},
      {"id":"dmx","type":"Demultiplexer","numPorts":3},
      {"id":"out","type":"Output","portTypes":["number","number","number"]}
    ],
    "connections":[
      {"from":"in","to":"data","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"c1","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"c2","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"c3","fromPort":"o1","toPort":"i1"},
      {"from":"data","to":"dmx","fromPort":"o1","toPort":"i1"},
      {"from":"c1","to":"dmx","fromPort":"o1","toPort":"c1"},
      {"from":"c2","to":"dmx","fromPort":"o1","toPort":"c2"},
      {"from":"c3","to":"dmx","fromPort":"o1","toPort":"c3"},
      {"from":"dmx","to":"out","fromPort":"o1","toPort":"i1"},
      {"from":"dmx","to":"out","fromPort":"o2","toPort":"i2"},
      {"from":"dmx","to":"out","fromPort":"o3","toPort":"i3"}
    ]
  })";
  require_jit_engaged(kJson);

  // Mix positive, zero, negative, and >=1 values across 200 ticks. Seed:
  // 0x45678.
  std::vector<rtbot::test::TickInput> inputs;
  std::mt19937_64 rng(0x45678ULL);
  std::uniform_real_distribution<double> dist(-1.0, 3.0);
  for (std::int64_t t = 1; t <= 200; ++t) {
    inputs.push_back({t, {dist(rng)}});
  }

  GIVEN("a Demux with 3 controls firing concurrently for many ticks") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

// ===========================================================================
// Tier 2: MovingKeyCount windowed eviction stress.
// ===========================================================================

SCENARIO("audit2 parity: MovingKeyCount W=10 high-cardinality long sequence",
         "[program_audit2][parity][moving_key_count]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"mkc","type":"MovingKeyCount","window_size":10},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"mkc","fromPort":"o1","toPort":"i1"},
      {"from":"mkc","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  // 800 ticks cycling through 25 keys -- ample eviction churn for W=10.
  std::vector<rtbot::test::TickInput> inputs;
  for (std::int64_t t = 1; t <= 800; ++t) {
    inputs.push_back({t, {static_cast<double>((t * 7) % 25)}});
  }

  GIVEN("MovingKeyCount W=10 driven by a long key sequence with eviction") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

SCENARIO("audit2 parity: MovingKeyCount W=4 frequent eviction",
         "[program_audit2][parity][moving_key_count]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"mkc","type":"MovingKeyCount","window_size":4},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"mkc","fromPort":"o1","toPort":"i1"},
      {"from":"mkc","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  // 600 ticks alternating between 8 keys with deterministic walk.
  std::vector<rtbot::test::TickInput> inputs;
  std::mt19937_64 rng(0x55556ULL);
  std::uniform_int_distribution<int> idist(0, 7);
  for (std::int64_t t = 1; t <= 600; ++t) {
    inputs.push_back({t, {static_cast<double>(idist(rng))}});
  }

  GIVEN("MovingKeyCount W=4 with 8-key churn driving constant eviction") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

// ===========================================================================
// Tier 3: NaN and +/-Inf in input streams.
// ===========================================================================

SCENARIO("audit2 parity: NaN / +/-Inf through scalar arithmetic chain",
         "[program_audit2][parity][nan_inf]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"sc","type":"Scale","value":2.0},
      {"id":"ad","type":"Add","value":-1.5},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"sc","fromPort":"o1","toPort":"i1"},
      {"from":"sc","to":"ad","fromPort":"o1","toPort":"i1"},
      {"from":"ad","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  const double NaN = std::numeric_limits<double>::quiet_NaN();
  const double Inf = std::numeric_limits<double>::infinity();
  std::vector<rtbot::test::TickInput> inputs = {
      {1,  {1.0}},  {2,  {NaN}}, {3,  {Inf}},  {4,  {-Inf}},
      {5,  {0.0}},  {6,  {-0.0}}, {7,  {NaN}}, {8,  {Inf}},
      {9,  {3.14}}, {10, {-Inf}},
  };

  GIVEN("a Scale+Add chain receiving NaN and +/-Inf samples") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

SCENARIO("audit2 parity: NaN / +/-Inf through windowed MovingAverage",
         "[program_audit2][parity][nan_inf]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"ma","type":"MovingAverage","window_size":3},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"ma","fromPort":"o1","toPort":"i1"},
      {"from":"ma","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  const double NaN = std::numeric_limits<double>::quiet_NaN();
  const double Inf = std::numeric_limits<double>::infinity();
  std::vector<rtbot::test::TickInput> inputs = {
      {1,  {1.0}},  {2,  {2.0}},  {3,  {3.0}},
      {4,  {NaN}},  {5,  {4.0}},  {6,  {5.0}},
      {7,  {Inf}},  {8,  {6.0}},  {9,  {-Inf}},
      {10, {7.0}},  {11, {8.0}},  {12, {9.0}},
  };

  GIVEN("MA(3) over a stream containing NaN and +/-Inf samples") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

// ===========================================================================
// Tier 3: Long-run accumulator drift.
// ===========================================================================

SCENARIO("audit2 parity: CumulativeSum over 12000 ticks drift check",
         "[program_audit2][parity][long_run][drift]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"cs","type":"CumulativeSum"},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"cs","fromPort":"o1","toPort":"i1"},
      {"from":"cs","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  // Mix of small + large magnitudes to exercise Kahan compensation. Seed:
  // 0x77777.
  std::vector<rtbot::test::TickInput> inputs;
  inputs.reserve(12000);
  std::mt19937_64 rng(0x77777ULL);
  std::uniform_real_distribution<double> dist_small(-1.0, 1.0);
  std::uniform_real_distribution<double> dist_large(-1e6, 1e6);
  for (std::int64_t t = 1; t <= 12000; ++t) {
    const double v = (t % 100 == 0) ? dist_large(rng) : dist_small(rng);
    inputs.push_back({t, {v}});
  }

  GIVEN("CumSum over 12k ticks alternating small and large magnitudes") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

SCENARIO("audit2 parity: MovingSum W=64 over 10000 ticks",
         "[program_audit2][parity][long_run][drift]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"ms","type":"MovingSum","window_size":64},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"ms","fromPort":"o1","toPort":"i1"},
      {"from":"ms","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  auto inputs = sine(10000, 5.0, 0.011);

  GIVEN("MovingSum W=64 over 10k sine samples") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

// ===========================================================================
// Tier 3: Single-tick program.
// ===========================================================================

SCENARIO("audit2 parity: single-tick MA program",
         "[program_audit2][parity][single_tick]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"ma","type":"MovingAverage","window_size":3},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"ma","fromPort":"o1","toPort":"i1"},
      {"from":"ma","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  std::vector<rtbot::test::TickInput> inputs = {{1, {42.5}}};

  GIVEN("a single-sample stream to MA(3) (warmup, no emission)") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

SCENARIO("audit2 parity: single-tick Identity program",
         "[program_audit2][parity][single_tick]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"id1","type":"Identity"},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"id1","fromPort":"o1","toPort":"i1"},
      {"from":"id1","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  std::vector<rtbot::test::TickInput> inputs = {{1, {3.14159}}};

  GIVEN("a single-sample Identity stream (one emission expected)") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

// ===========================================================================
// Tier 3: Timestamps near INT64 boundaries.
// ===========================================================================

SCENARIO("audit2 parity: large-int64 timestamps (1e18 range)",
         "[program_audit2][parity][int64_boundary]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"ma","type":"MovingAverage","window_size":4},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"ma","fromPort":"o1","toPort":"i1"},
      {"from":"ma","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  std::vector<rtbot::test::TickInput> inputs;
  const std::int64_t t0 = static_cast<std::int64_t>(1'000'000'000'000'000'000LL);
  std::mt19937_64 rng(0x99999ULL);
  std::uniform_real_distribution<double> dist(-5.0, 5.0);
  for (int i = 0; i < 64; ++i) {
    inputs.push_back({t0 + i, {dist(rng)}});
  }

  GIVEN("MA(4) driven with timestamps near 1e18") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

// ===========================================================================
// Mid-tier: Linear with N=5 inputs.
// ===========================================================================

SCENARIO("audit2 parity: Linear with 5 input ports",
         "[program_audit2][parity][linear]") {
  const char* kJson = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"b1","type":"Identity"},
      {"id":"b2","type":"Add","value":1.0},
      {"id":"b3","type":"Add","value":2.0},
      {"id":"b4","type":"Scale","value":0.5},
      {"id":"b5","type":"Scale","value":-1.0},
      {"id":"lin","type":"Linear","coefficients":[0.5,0.2,0.1,0.1,0.1]},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"b1","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b2","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b3","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b4","fromPort":"o1","toPort":"i1"},
      {"from":"in","to":"b5","fromPort":"o1","toPort":"i1"},
      {"from":"b1","to":"lin","fromPort":"o1","toPort":"i1"},
      {"from":"b2","to":"lin","fromPort":"o1","toPort":"i2"},
      {"from":"b3","to":"lin","fromPort":"o1","toPort":"i3"},
      {"from":"b4","to":"lin","fromPort":"o1","toPort":"i4"},
      {"from":"b5","to":"lin","fromPort":"o1","toPort":"i5"},
      {"from":"lin","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
  require_jit_engaged(kJson);

  auto inputs = mt_random(150, 0xBABA1ULL);

  GIVEN("a 5-port Linear with shift branches and weighted combination") {
    rtbot::test::run_jit_vs_interpreter_parity(kJson, inputs, send_scalar());
  }
}

// ===========================================================================
// Mid-tier: ReduceJoin variants (Subtraction, Multiplication, Division,
// Addition, LogicalAnd/Or/Xor/Nand/Nor/Implication). Each variant tested via
// a shared JSON template; the reducer requires `numPorts` when the lane
// count is anything other than 2.
// ===========================================================================

namespace {

// Build a graph: scalar Input -> N branches -> ReduceJoin(num_ports). For
// arithmetic reducers the branches are Identity / Add / Scale ops emitting
// NumberData. For boolean reducers we route through CompareScalar (emits
// BooleanData) and feed the boolean lanes directly to the reducer.
std::string make_reduce_json(const std::string& op_type, std::size_t num_ports,
                              bool boolean_input) {
  std::string j;
  j  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  for (std::size_t i = 0; i < num_ports; ++i) {
    if (boolean_input) {
      const double thr = -1.0 + static_cast<double>(i) * 0.5;
      j += R"({"id":"b)" + std::to_string(i) +
           R"(","type":"CompareGT","value":)" + std::to_string(thr) + R"(},)";
    } else if (i == 0) {
      j += R"({"id":"b0","type":"Identity"},)";
    } else {
      const double shift = static_cast<double>(i);
      j += R"({"id":"b)" + std::to_string(i) +
           R"(","type":"Add","value":)" + std::to_string(shift) + R"(},)";
    }
  }
  j += R"({"id":"red","type":")" + op_type +
       R"(","numPorts":)" + std::to_string(num_ports) + R"(},)";
  // Boolean reducers emit BooleanData; bridge through BooleanToNumber so the
  // Output port type (number) matches.
  const std::string out_port_type = boolean_input ? "number" : "number";
  if (boolean_input) {
    j += R"({"id":"b2n","type":"BooleanToNumber"},)";
  }
  j += R"({"id":"out","type":"Output","portTypes":[")" + out_port_type +
       R"("]}],)";
  j += R"("connections":[)";
  for (std::size_t i = 0; i < num_ports; ++i) {
    j += R"({"from":"in","to":"b)" + std::to_string(i) +
         R"(","fromPort":"o1","toPort":"i1"},)";
    j += R"({"from":"b)" + std::to_string(i) +
         R"(","to":"red","fromPort":"o1","toPort":"i)" +
         std::to_string(i + 1) + R"("},)";
  }
  if (boolean_input) {
    j += R"({"from":"red","to":"b2n","fromPort":"o1","toPort":"i1"},)";
    j += R"({"from":"b2n","to":"out","fromPort":"o1","toPort":"i1"})";
  } else {
    j += R"({"from":"red","to":"out","fromPort":"o1","toPort":"i1"})";
  }
  j += R"(]})";
  return j;
}

}  // namespace

SCENARIO("audit2 parity: ReduceJoin arithmetic family variants",
         "[program_audit2][parity][reduce_join]") {
  auto inputs = mt_random(80, 0xCAFEULL, -3.0, 3.0);

  GIVEN("Addition over 3 ports") {
    const auto json = make_reduce_json("Addition", 3, false);
    require_jit_engaged(json);
    rtbot::test::run_jit_vs_interpreter_parity(json, inputs, send_scalar());
  }
  GIVEN("Subtraction over 3 ports") {
    const auto json = make_reduce_json("Subtraction", 3, false);
    require_jit_engaged(json);
    rtbot::test::run_jit_vs_interpreter_parity(json, inputs, send_scalar());
  }
  GIVEN("Multiplication over 3 ports") {
    const auto json = make_reduce_json("Multiplication", 3, false);
    require_jit_engaged(json);
    rtbot::test::run_jit_vs_interpreter_parity(json, inputs, send_scalar());
  }
  GIVEN("Division over 3 ports (positive lanes only to avoid /0)") {
    auto positives = mt_random(80, 0xBEEF1ULL, 2.0, 10.0);
    const auto json = make_reduce_json("Division", 3, false);
    require_jit_engaged(json);
    rtbot::test::run_jit_vs_interpreter_parity(json, positives,
                                                send_scalar());
  }
}

SCENARIO("audit2 parity: BooleanSync 6 reductions",
         "[program_audit2][parity][boolean_sync]") {
  auto inputs = mt_random(120, 0xDEADBULL, -2.0, 2.0);

  for (const char* op_name :
       {"LogicalAnd", "LogicalOr", "LogicalXor", "LogicalNand", "LogicalNor",
        "LogicalImplication"}) {
    GIVEN(std::string("BooleanSync variant: ") + op_name) {
      const auto json = make_reduce_json(op_name, 3, /*boolean_input=*/true);
      require_jit_engaged(json);
      rtbot::test::run_jit_vs_interpreter_parity(json, inputs, send_scalar());
    }
  }
}

// ===========================================================================
// Mid-tier: ArithmeticScalar variants. Each variant is a single-input
// transformation.
// ===========================================================================

namespace {

std::string make_arith_scalar_json(const std::string& op_type,
                                    double value, bool has_value) {
  std::string j;
  j  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  j += R"({"id":"op","type":")" + op_type + R"(")";
  if (has_value) j += R"(,"value":)" + std::to_string(value);
  j += R"(},)";
  j += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  j += R"("connections":[)";
  j += R"({"from":"in","to":"op","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

}  // namespace

SCENARIO("audit2 parity: ArithmeticScalar value-bearing variants",
         "[program_audit2][parity][arithmetic_scalar]") {
  auto wide = mt_random(64, 0xE1E1ULL, -3.0, 3.0);

  GIVEN("Add(value=2.5)") {
    const auto json = make_arith_scalar_json("Add", 2.5, true);
    require_jit_engaged(json);
    rtbot::test::run_jit_vs_interpreter_parity(json, wide, send_scalar());
  }
  GIVEN("Scale(value=-1.25)") {
    const auto json = make_arith_scalar_json("Scale", -1.25, true);
    require_jit_engaged(json);
    rtbot::test::run_jit_vs_interpreter_parity(json, wide, send_scalar());
  }
  GIVEN("Power(value=3.0)") {
    const auto json = make_arith_scalar_json("Power", 3.0, true);
    require_jit_engaged(json);
    rtbot::test::run_jit_vs_interpreter_parity(json, wide, send_scalar());
  }
}

SCENARIO("audit2 parity: ArithmeticScalar transcendental family",
         "[program_audit2][parity][arithmetic_scalar]") {
  auto wide = mt_random(64, 0xE2E2ULL, -3.0, 3.0);

  for (const char* op_name :
       {"Sin", "Cos", "Tan", "Abs", "Sign", "Floor", "Ceil", "Round", "Exp"}) {
    GIVEN(std::string("ArithmeticScalar variant: ") + op_name) {
      const auto json = make_arith_scalar_json(op_name, 0.0, false);
      require_jit_engaged(json);
      rtbot::test::run_jit_vs_interpreter_parity(json, wide, send_scalar());
    }
  }
}

SCENARIO("audit2 parity: ArithmeticScalar log family on positive inputs",
         "[program_audit2][parity][arithmetic_scalar]") {
  auto positive = mt_random(64, 0xE3E3ULL, 0.5, 5.0);

  GIVEN("Log on strictly-positive samples") {
    const auto json = make_arith_scalar_json("Log", 0.0, false);
    require_jit_engaged(json);
    rtbot::test::run_jit_vs_interpreter_parity(json, positive, send_scalar());
  }
  GIVEN("Log10 on strictly-positive samples") {
    const auto json = make_arith_scalar_json("Log10", 0.0, false);
    require_jit_engaged(json);
    rtbot::test::run_jit_vs_interpreter_parity(json, positive, send_scalar());
  }
}

// ===========================================================================
// Mid-tier: CompareScalar variants. CompareScalar emits BooleanData; we
// bridge through BooleanToNumber so the Output port type matches.
// ===========================================================================

namespace {

std::string make_compare_scalar_json(const std::string& op_type, double value) {
  std::string j;
  j  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  j += R"({"id":"op","type":")" + op_type +
       R"(","value":)" + std::to_string(value) + R"(},)";
  j += R"({"id":"b2n","type":"BooleanToNumber"},)";
  j += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  j += R"("connections":[)";
  j += R"({"from":"in","to":"op","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"op","to":"b2n","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"b2n","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

}  // namespace

SCENARIO("audit2 parity: CompareScalar variants",
         "[program_audit2][parity][compare_scalar]") {
  auto inputs = mt_random(80, 0xF1F1ULL, -2.0, 2.0);

  for (const char* op_name : {"CompareGT", "CompareLT", "CompareGTE",
                                "CompareLTE", "CompareEQ", "CompareNEQ"}) {
    GIVEN(std::string("CompareScalar variant: ") + op_name) {
      const auto json = make_compare_scalar_json(op_name, 0.0);
      require_jit_engaged(json);
      rtbot::test::run_jit_vs_interpreter_parity(json, inputs, send_scalar());
    }
  }
}

// ===========================================================================
// Mid-tier: CompareSync variants. Two inputs (Identity + shift), boolean
// output bridged via BooleanToNumber.
// ===========================================================================

namespace {

std::string make_compare_sync_json(const std::string& op_type,
                                    std::size_t num_ports) {
  std::string j;
  j  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  for (std::size_t i = 0; i < num_ports; ++i) {
    if (i == 0) {
      j += R"({"id":"b0","type":"Identity"},)";
    } else {
      const double shift = static_cast<double>(i) * 0.5;
      j += R"({"id":"b)" + std::to_string(i) +
           R"(","type":"Add","value":)" + std::to_string(shift) + R"(},)";
    }
  }
  j += R"({"id":"op","type":")" + op_type +
       R"(","numPorts":)" + std::to_string(num_ports) + R"(},)";
  j += R"({"id":"b2n","type":"BooleanToNumber"},)";
  j += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  j += R"("connections":[)";
  for (std::size_t i = 0; i < num_ports; ++i) {
    j += R"({"from":"in","to":"b)" + std::to_string(i) +
         R"(","fromPort":"o1","toPort":"i1"},)";
    j += R"({"from":"b)" + std::to_string(i) +
         R"(","to":"op","fromPort":"o1","toPort":"i)" +
         std::to_string(i + 1) + R"("},)";
  }
  j += R"({"from":"op","to":"b2n","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"b2n","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

}  // namespace

SCENARIO("audit2 parity: CompareSync variants",
         "[program_audit2][parity][compare_sync]") {
  auto inputs = mt_random(80, 0xF2F2ULL, -2.0, 2.0);

  for (const char* op_name : {"CompareSyncGT", "CompareSyncLT",
                                "CompareSyncGTE", "CompareSyncLTE",
                                "CompareSyncEQ", "CompareSyncNEQ"}) {
    GIVEN(std::string("CompareSync variant: ") + op_name) {
      const auto json = make_compare_sync_json(op_name, 2);
      require_jit_engaged(json);
      rtbot::test::run_jit_vs_interpreter_parity(json, inputs, send_scalar());
    }
  }
}

// ===========================================================================
// Gated-aggregate: Filter upstream suppresses aggregate state updates.
// Covers the Fix-B bug where CumSum/Count/MaxAgg/MinAgg accumulated on every
// tick regardless of whether the upstream Filter emitted.
// ===========================================================================

// Input → GreaterThan(0) → CumulativeSum → Output
// Only positive ticks cross the filter; the cumsum must match the framework.
SCENARIO("audit2 parity: Filter -> CumSum only accumulates passing ticks",
         "[program_audit2][parity][gated_aggregate][fix_b]") {
  // Mix of positive and negative values so ~half the ticks are suppressed.
  auto inputs = mt_random(60, 0xA1B2C3ULL, -5.0, 5.0);

  const char* json = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"filt","type":"GreaterThan","value":0.0},
      {"id":"cs","type":"CumulativeSum"},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"filt","fromPort":"o1","toPort":"i1"},
      {"from":"filt","to":"cs","fromPort":"o1","toPort":"i1"},
      {"from":"cs","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";

  require_jit_engaged(json);
  GIVEN("positive-only ticks pass the filter and accumulate") {
    rtbot::test::run_jit_vs_interpreter_parity(json, inputs, send_scalar());
  }
}

// Input → LessThan(3) → CountNumber → Output
// Only ticks below 3 count; verifies gated count matches framework.
SCENARIO("audit2 parity: Filter -> Count only counts passing ticks",
         "[program_audit2][parity][gated_aggregate][fix_b]") {
  // Values in [-1, 6]: some pass LessThan(3), some don't.
  auto inputs = mt_random(50, 0xD4E5F6ULL, -1.0, 6.0);

  const char* json = R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"filt","type":"LessThan","value":3.0},
      {"id":"cnt","type":"CountNumber"},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"filt","fromPort":"o1","toPort":"i1"},
      {"from":"filt","to":"cnt","fromPort":"o1","toPort":"i1"},
      {"from":"cnt","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";

  require_jit_engaged(json);
  GIVEN("sub-threshold ticks are counted, rest suppressed") {
    rtbot::test::run_jit_vs_interpreter_parity(json, inputs, send_scalar());
  }
}

// Input → GreaterThan(-2) → MaxTracker → Output
// Input → GreaterThan(-2) → MinTracker → Output
// MaxTracker/MinTracker are not in the top-level Program JSON schema, so this
// test drives the JIT compiler directly and compares against the framework's
// operator API.
SCENARIO("audit2 parity: Filter -> MaxTracker and MinTracker only update on passing ticks",
         "[program_audit2][parity][gated_aggregate][fix_b]") {
  // Values in [-5, 5]: some below -2 (suppressed), rest pass the filter.
  std::vector<std::pair<std::int64_t, double>> inputs;
  {
    std::mt19937_64 rng(0x7F8E9DULL);
    std::uniform_real_distribution<double> dist(-5.0, 5.0);
    for (int i = 1; i <= 60; ++i)
      inputs.push_back({static_cast<std::int64_t>(i), dist(rng)});
  }

  auto dbits = [](double v) -> std::uint64_t {
    std::uint64_t u = 0;
    std::memcpy(&u, &v, sizeof(double));
    return u;
  };

  // MaxTracker: JIT vs framework.
  GIVEN("MaxTracker extrema only updated for ticks passing the filter") {
    const std::string json =
      R"({"title":"t","apiVersion":"v1","entryOperator":"in",)"
      R"("output":{"out":["o1"]},"operators":[)"
      R"({"id":"in","type":"Input","portTypes":["number"]},)"
      R"({"id":"filt","type":"GreaterThan","value":-2.0},)"
      R"({"id":"maxt","type":"MaxTracker"},)"
      R"({"id":"out","type":"Output","portTypes":["number"]}],)"
      R"("connections":[)"
      R"({"from":"in","to":"filt","fromPort":"o1","toPort":"i1"},)"
      R"({"from":"filt","to":"maxt","fromPort":"o1","toPort":"i1"},)"
      R"({"from":"maxt","to":"out","fromPort":"o1","toPort":"i1"}]})";

    rtbot::jit::JitCompiler compiler;
    auto jit_prog = compiler.compile(json);
    REQUIRE(jit_prog != nullptr);
    for (const auto& [t, v] : inputs) jit_prog->receive(t, v);
    auto jit_out = jit_prog->collect_outputs();

    auto filt_op = rtbot::make_greater_than("filt", -2.0);
    auto maxt_op = rtbot::make_max_tracker("maxt");
    auto sink    = std::make_shared<rtbot::Collector>(
        "sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    filt_op->connect(maxt_op, 0, 0);
    maxt_op->connect(sink, 0, 0);
    for (const auto& [t, v] : inputs) {
      filt_op->receive_data(
          rtbot::create_message<rtbot::NumberData>(t, rtbot::NumberData{v}), 0);
    }
    filt_op->execute();
    auto& fe_q = sink->get_data_queue(0);
    REQUIRE(jit_out.size() == fe_q.size());
    for (std::size_t i = 0; i < jit_out.size(); ++i) {
      const auto* fe_msg =
          static_cast<const rtbot::Message<rtbot::NumberData>*>(fe_q[i].get());
      INFO("index " << i);
      REQUIRE(jit_out[i].time == fe_msg->time);
      REQUIRE(dbits(jit_out[i].values[0]) == dbits(fe_msg->data.value));
    }
  }

  // MinTracker: JIT vs framework.
  GIVEN("MinTracker extrema only updated for ticks passing the filter") {
    const std::string json =
      R"({"title":"t","apiVersion":"v1","entryOperator":"in",)"
      R"("output":{"out":["o1"]},"operators":[)"
      R"({"id":"in","type":"Input","portTypes":["number"]},)"
      R"({"id":"filt","type":"GreaterThan","value":-2.0},)"
      R"({"id":"mint","type":"MinTracker"},)"
      R"({"id":"out","type":"Output","portTypes":["number"]}],)"
      R"("connections":[)"
      R"({"from":"in","to":"filt","fromPort":"o1","toPort":"i1"},)"
      R"({"from":"filt","to":"mint","fromPort":"o1","toPort":"i1"},)"
      R"({"from":"mint","to":"out","fromPort":"o1","toPort":"i1"}]})";

    rtbot::jit::JitCompiler compiler;
    auto jit_prog = compiler.compile(json);
    REQUIRE(jit_prog != nullptr);
    for (const auto& [t, v] : inputs) jit_prog->receive(t, v);
    auto jit_out = jit_prog->collect_outputs();

    auto filt_op = rtbot::make_greater_than("filt", -2.0);
    auto mint_op = rtbot::make_min_tracker("mint");
    auto sink    = std::make_shared<rtbot::Collector>(
        "sink", std::vector<std::string>{rtbot::PortType::NUMBER});
    filt_op->connect(mint_op, 0, 0);
    mint_op->connect(sink, 0, 0);
    for (const auto& [t, v] : inputs) {
      filt_op->receive_data(
          rtbot::create_message<rtbot::NumberData>(t, rtbot::NumberData{v}), 0);
    }
    filt_op->execute();
    auto& fe_q = sink->get_data_queue(0);
    REQUIRE(jit_out.size() == fe_q.size());
    for (std::size_t i = 0; i < jit_out.size(); ++i) {
      const auto* fe_msg =
          static_cast<const rtbot::Message<rtbot::NumberData>*>(fe_q[i].get());
      INFO("index " << i);
      REQUIRE(jit_out[i].time == fe_msg->time);
      REQUIRE(dbits(jit_out[i].values[0]) == dbits(fe_msg->data.value));
    }
  }
}
