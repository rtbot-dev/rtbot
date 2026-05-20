// test_framework_parity.cpp
//
// Bit-exact parity between the JIT execution path and the canonical
// operator-graph framework path (Program::send_to_entry → operator propagation,
// engaged when force_interpreter_for_testing is on or RTBOT_DISABLE_JIT=1).
//
// Each SCENARIO mirrors an existing Program-bearing framework test in
// libs/api/test/. The framework test asserts behavior via REQUIRE/Approx; this
// file additionally pins per-emission bit-exact equality between modes via
// parity_helper::run_jit_vs_interpreter_parity.
//
// Coverage targets:
//   - test_program.cpp basic Input→Output graph
//   - test_program.cpp Input→MovingAverage→Output
//   - test_program.cpp Pipeline (MA+StdDev) with control port
//   - test_trigger_set.cpp TriggerSet (MA+StdDev)
//   - integration_test_rsi_program.cpp RSI(14) full graph
//   - prototype expansion + nested pipeline shapes
//
// If JIT cannot lower a shape it falls back to the interpreter; in that case
// the parity helper compares interpreter-vs-interpreter, which is still a
// useful invariant (drain_into wire format remains identical across runs).

#include <catch2/catch.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "rtbot/Program.h"
#include "parity_helper.h"

namespace {

const char* kBasicInputOutputJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

const char* kInputMaOutputJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "MovingAverage", "id": "ma1", "window_size": 3},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
    {"from": "ma1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

const char* kStdDevWindowJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "StandardDeviation", "id": "std1", "window_size": 5},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "std1", "fromPort": "o1", "toPort": "i1"},
    {"from": "std1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

const char* kMaPlusScaleJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "MovingAverage", "id": "ma1", "window_size": 4},
    {"type": "Scale", "id": "scale1", "value": 2.5},
    {"type": "Add", "id": "add1", "value": 1.0},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
    {"from": "ma1", "to": "scale1", "fromPort": "o1", "toPort": "i1"},
    {"from": "scale1", "to": "add1", "fromPort": "o1", "toPort": "i1"},
    {"from": "add1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

const char* kCumulativeSumJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "CumulativeSum", "id": "cs1"},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "cs1", "fromPort": "o1", "toPort": "i1"},
    {"from": "cs1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

const char* kPeakDetectorJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "PeakDetector", "id": "pk1", "window_size": 5},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "pk1", "fromPort": "o1", "toPort": "i1"},
    {"from": "pk1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// Pipeline with key-driven control port. Mirrors test_program.cpp pipeline
// scenario. Pipeline is interpreter-only today; parity helper degrades to
// interp-vs-interp, still a useful invariant.
const char* kPipelineMaStdJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
    {
      "type": "Pipeline",
      "id": "pipeline1",
      "input_port_types": ["number"],
      "output_port_types": ["number"],
      "operators": [
        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
        {"type": "StandardDeviation", "id": "std1", "window_size": 3}
      ],
      "connections": [
        {"from": "ma1", "to": "std1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "ma1",
      "outputMappings": { "std1": {"o1": "o1"} }
    },
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "pipeline1", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "pipeline1", "fromPort": "o2", "toPort": "c1"},
    {"from": "pipeline1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

const char* kTriggerSetMaStdJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {
      "type": "TriggerSet",
      "id": "ts1",
      "input_port_type": "number",
      "output_port_type": "number",
      "operators": [
        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
        {"type": "StandardDeviation", "id": "std1", "window_size": 3}
      ],
      "connections": [
        {"from": "ma1", "to": "std1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "ma1",
      "outputOperator": {"id": "std1", "port": "o1"}
    },
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "ts1", "fromPort": "o1", "toPort": "i1"},
    {"from": "ts1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// Mirrors integration_test_rsi_program.cpp create_rsi_program(14). Inlined
// here so the JSON travels with the parity SCENARIO. n=14 → alpha=1/14,
// beta=13/14. The RSI graph mixes Variable, TimeShift, Demultiplexer and
// CountNumber — most fall to interpreter today; parity helper still asserts
// per-tick determinism.
const char* kRsi14Json = R"({
  "operators": [
    {"id": "input", "type": "Input", "portTypes": ["number"]},
    {"id": "output", "type": "Output", "portTypes": ["number"]},
    {"id": "count", "type": "CountNumber"},
    {"id": "dm", "type": "Demultiplexer", "numPorts": 2},
    {"id": "lt", "type": "LessThan", "value": 15},
    {"id": "et", "type": "EqualTo", "value": 15},
    {"id": "gt", "type": "GreaterThan", "value": 15},
    {"id": "etn2", "type": "EqualTo", "value": 16},
    {"id": "diff1", "type": "Difference"},
    {"id": "diff2", "type": "Difference"},
    {"id": "gt0", "type": "GreaterThan", "value": 0.0},
    {"id": "et0", "type": "EqualTo", "value": 0.0},
    {"id": "lt0", "type": "LessThan", "value": 0.0},
    {"id": "sum0", "type": "CumulativeSum"},
    {"id": "sc0", "type": "Scale", "value": 0.07142857142857142},
    {"id": "l1", "type": "Linear", "coefficients": [0.9285714285714286, 0.07142857142857142]},
    {"id": "l2", "type": "Linear", "coefficients": [0.9285714285714286, 0.07142857142857142]},
    {"id": "neg0", "type": "Scale", "value": -1.0},
    {"id": "sum1", "type": "CumulativeSum"},
    {"id": "sc1", "type": "Scale", "value": 0.07142857142857142},
    {"id": "gt1", "type": "GreaterThan", "value": 0.0},
    {"id": "et1", "type": "EqualTo", "value": 0.0},
    {"id": "lt1", "type": "LessThan", "value": 0.0},
    {"id": "const0", "type": "ConstantNumber", "value": 0.0},
    {"id": "const1", "type": "ConstantNumber", "value": 0.0},
    {"id": "neg1", "type": "Scale", "value": -1.0},
    {"id": "varg", "type": "Variable", "default_value": 0.0},
    {"id": "varl", "type": "Variable", "default_value": 0.0},
    {"id": "ts1", "type": "TimeShift", "shift": 1},
    {"id": "ts11", "type": "TimeShift", "shift": 1},
    {"id": "ts2", "type": "TimeShift", "shift": 1},
    {"id": "ts22", "type": "TimeShift", "shift": 1},
    {"id": "divide", "type": "Division"},
    {"id": "add1", "type": "Add", "value": 1.0},
    {"id": "power_1", "type": "Power", "value": -1.0},
    {"id": "scale_100", "type": "Scale", "value": -100.0},
    {"id": "add100", "type": "Add", "value": 100.0},
    {"id": "cgtz", "type": "ConstantNumberToBoolean", "value": false},
    {"id": "cgto", "type": "ConstantNumberToBoolean", "value": true},
    {"id": "cltz", "type": "ConstantNumberToBoolean", "value": false},
    {"id": "clto", "type": "ConstantNumberToBoolean", "value": true},
    {"id": "ceto", "type": "ConstantNumberToBoolean", "value": true}
  ],
  "connections": [
    {"from": "input", "to": "dm", "fromPort": "o1", "toPort": "i1"},
    {"from": "input", "to": "count"},
    {"from": "count", "to": "lt"},
    {"from": "count", "to": "gt"},
    {"from": "count", "to": "et"},
    {"from": "count", "to": "etn2"},
    {"from": "lt", "to": "clto"},
    {"from": "clto", "to": "dm", "toPort": "c1"},
    {"from": "lt", "to": "cltz"},
    {"from": "cltz", "to": "dm", "toPort": "c2"},
    {"from": "et", "to": "ceto"},
    {"from": "ceto", "to": "dm", "toPort": "c1"},
    {"from": "ceto", "to": "dm", "toPort": "c2"},
    {"from": "gt", "to": "cgto"},
    {"from": "gt", "to": "cgtz"},
    {"from": "cgto", "to": "dm", "toPort": "c2"},
    {"from": "cgtz", "to": "dm", "toPort": "c1"},
    {"from": "dm", "to": "diff1", "fromPort": "o1", "toPort": "i1"},
    {"from": "diff1", "to": "gt0"},
    {"from": "gt0", "to": "sum0"},
    {"from": "sum0", "to": "sc0"},
    {"from": "sc0", "to": "varg"},
    {"from": "varg", "to": "ts1"},
    {"from": "ts1", "to": "l1", "toPort": "i1"},
    {"from": "l1", "to": "ts11"},
    {"from": "ts11", "to": "l1"},
    {"from": "diff1", "to": "et0"},
    {"from": "et0", "to": "sum0"},
    {"from": "et0", "to": "sum1"},
    {"from": "diff1", "to": "lt0"},
    {"from": "lt0", "to": "neg0"},
    {"from": "neg0", "to": "sum1"},
    {"from": "sum1", "to": "sc1"},
    {"from": "sc1", "to": "varl"},
    {"from": "varl", "to": "ts2"},
    {"from": "ts2", "to": "l2", "toPort": "i1"},
    {"from": "l2", "to": "ts22"},
    {"from": "ts22", "to": "l2"},
    {"from": "dm", "to": "diff2", "fromPort": "o2", "toPort": "i1"},
    {"from": "diff2", "to": "gt1"},
    {"from": "gt1", "to": "l1", "toPort": "i2"},
    {"from": "gt1", "to": "const0"},
    {"from": "const0", "to": "l2", "toPort": "i2"},
    {"from": "diff2", "to": "lt1"},
    {"from": "lt1", "to": "neg1"},
    {"from": "neg1", "to": "l2", "toPort": "i2"},
    {"from": "lt1", "to": "const1"},
    {"from": "const1", "to": "l1", "toPort": "i2"},
    {"from": "diff2", "to": "et1"},
    {"from": "et1", "to": "l1", "toPort": "i2"},
    {"from": "et1", "to": "l2", "toPort": "i2"},
    {"from": "etn2", "to": "varg"},
    {"from": "etn2", "to": "varl"},
    {"from": "et", "to": "varg", "toPort": "c1"},
    {"from": "et", "to": "varl", "toPort": "c1"},
    {"from": "varg", "to": "divide", "toPort": "i1"},
    {"from": "varl", "to": "divide", "toPort": "i2"},
    {"from": "l1", "to": "divide", "toPort": "i1"},
    {"from": "l2", "to": "divide", "toPort": "i2"},
    {"from": "divide", "to": "add1"},
    {"from": "add1", "to": "power_1"},
    {"from": "power_1", "to": "scale_100"},
    {"from": "scale_100", "to": "add100"},
    {"from": "add100", "to": "output"}
  ],
  "entryOperator": "input",
  "output": { "output": ["o1"] }
})";

std::vector<rtbot::test::TickInput> ramp_inputs(int count, double base, double step) {
  std::vector<rtbot::test::TickInput> out;
  out.reserve(count);
  for (int i = 1; i <= count; ++i) {
    out.push_back({static_cast<std::int64_t>(i), {base + step * static_cast<double>(i)}});
  }
  return out;
}

std::vector<rtbot::test::TickInput> sin_inputs(int count, double amplitude, double freq) {
  std::vector<rtbot::test::TickInput> out;
  out.reserve(count);
  for (int i = 1; i <= count; ++i) {
    out.push_back({static_cast<std::int64_t>(i),
                   {amplitude * std::sin(freq * static_cast<double>(i))}});
  }
  return out;
}

auto send_scalar() {
  return [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
    p.send(tick.t, tick.values[0]);
  };
}

}  // namespace

SCENARIO("framework parity: test_program basic Input->Output",
         "[program][jit][framework_parity]") {
  GIVEN("a 1-op passthrough program driven by a ramp") {
    auto inputs = ramp_inputs(64, 0.0, 1.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kBasicInputOutputJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: test_program Input->MovingAverage(3)->Output",
         "[program][jit][framework_parity]") {
  GIVEN("a 3-point moving average driven across the warmup boundary") {
    auto inputs = ramp_inputs(64, 0.0, 3.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kInputMaOutputJson, inputs, send_scalar());
  }
  GIVEN("the same MA(3) driven by a non-monotonic sine") {
    auto inputs = sin_inputs(200, 10.0, 0.13);
    rtbot::test::run_jit_vs_interpreter_parity(
        kInputMaOutputJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: StandardDeviation(5) end-to-end",
         "[program][jit][framework_parity]") {
  GIVEN("a 5-point std deviation driven by a sine") {
    auto inputs = sin_inputs(300, 5.0, 0.07);
    rtbot::test::run_jit_vs_interpreter_parity(
        kStdDevWindowJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: MA->Scale->Add chained scalars",
         "[program][jit][framework_parity]") {
  GIVEN("a four-op chain across the warmup boundary") {
    auto inputs = ramp_inputs(128, 1.5, 0.25);
    rtbot::test::run_jit_vs_interpreter_parity(
        kMaPlusScaleJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: CumulativeSum scalar",
         "[program][jit][framework_parity]") {
  GIVEN("a cumulative-sum chain driven by a sine") {
    auto inputs = sin_inputs(200, 3.0, 0.11);
    rtbot::test::run_jit_vs_interpreter_parity(
        kCumulativeSumJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: PeakDetector(5) scalar",
         "[program][jit][framework_parity]") {
  GIVEN("a peak detector driven by a noisy sine") {
    std::vector<rtbot::test::TickInput> inputs;
    inputs.reserve(300);
    for (int i = 1; i <= 300; ++i) {
      const double v = std::sin(static_cast<double>(i) * 0.31) +
                       0.3 * std::cos(static_cast<double>(i) * 0.07);
      inputs.push_back({static_cast<std::int64_t>(i), {v}});
    }
    rtbot::test::run_jit_vs_interpreter_parity(
        kPeakDetectorJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: Pipeline (MA+StdDev) with control port",
         "[program][jit][framework_parity][pipeline]") {
  GIVEN("a Pipeline driven with a stable key for warmup, then a key flip") {
    std::vector<rtbot::test::TickInput> inputs;
    inputs.reserve(40);
    // Pipeline scenario in test_program.cpp uses send(t, value, port) on i1/i2
    // independently; here we drive both ports per tick to match. The parity
    // helper drives once per tick, so we send the data via i1 and the control
    // via i2 inside the callback.
    for (int t = 1; t <= 20; ++t) {
      inputs.push_back({static_cast<std::int64_t>(t), {static_cast<double>(t * 3), 1.0}});
    }
    // Flip the key for the last few ticks to trigger a flush.
    for (int t = 21; t <= 26; ++t) {
      inputs.push_back({static_cast<std::int64_t>(t), {static_cast<double>(t * 3), 2.0}});
    }
    rtbot::test::run_jit_vs_interpreter_parity(
        kPipelineMaStdJson, inputs,
        [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
          p.send(tick.t, tick.values[0], "i1");
          p.send(tick.t, tick.values[1], "i2");
        });
  }
}

SCENARIO("framework parity: TriggerSet (MA+StdDev) end-to-end",
         "[program][jit][framework_parity][trigger_set]") {
  GIVEN("a TriggerSet wrapping a 3-point MA + 3-point StdDev") {
    auto inputs = ramp_inputs(12, 0.0, 3.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kTriggerSetMaStdJson, inputs, send_scalar());
  }
}

// Bollinger Bands pipeline (mirrors examples/data/program-test-6.json). The
// ResamplerHermite at interval=1 onto thinned input was emitting multiple
// ticks per input call but the JIT segment scratch only held the LAST one
// before this scenario was added; pinning bit-exact parity here keeps the
// resampler multi-emit ABI honest as the codebase evolves.
const char* kBollingerBandsJson = R"({
  "title": "Bollinger Bands",
  "entryOperator": "754",
  "output": { "37": ["o2", "o1", "o3"] },
  "operators": [
    { "id": "37", "type": "Output", "portTypes": ["number", "number", "number"] },
    { "id": "495", "type": "Subtraction" },
    { "id": "861", "type": "Addition" },
    { "id": "996", "type": "Scale", "value": 2 },
    { "id": "865", "type": "StandardDeviation", "window_size": 14 },
    { "id": "510", "type": "MovingAverage", "window_size": 14 },
    { "id": "262", "type": "ResamplerHermite", "interval": 1 },
    { "id": "754", "type": "Input", "portTypes": ["number"] }
  ],
  "connections": [
    { "from": "510", "to": "37", "fromPort": "o1", "toPort": "i3" },
    { "from": "495", "to": "37", "fromPort": "o1", "toPort": "i1" },
    { "from": "861", "to": "37", "fromPort": "o1", "toPort": "i2" },
    { "from": "996", "to": "495", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "495", "fromPort": "o1", "toPort": "i1" },
    { "from": "996", "to": "861", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "861", "fromPort": "o1", "toPort": "i1" },
    { "from": "865", "to": "996", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "865", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "510", "fromPort": "o1", "toPort": "i1" },
    { "from": "754", "to": "262", "fromPort": "o1", "toPort": "i1" }
  ]
})";

// Same shape but with a ResamplerConstant instead of ResamplerHermite.
// Covers the second resampler emitter through the same multi-emit ABI.
const char* kBollingerConstantJson = R"JSON({
  "title": "Bollinger Bands constant resampler",
  "entryOperator": "754",
  "output": { "37": ["o2", "o1", "o3"] },
  "operators": [
    { "id": "37", "type": "Output", "portTypes": ["number", "number", "number"] },
    { "id": "495", "type": "Subtraction" },
    { "id": "861", "type": "Addition" },
    { "id": "996", "type": "Scale", "value": 2 },
    { "id": "865", "type": "StandardDeviation", "window_size": 14 },
    { "id": "510", "type": "MovingAverage", "window_size": 14 },
    { "id": "262", "type": "ResamplerConstant", "interval": 1 },
    { "id": "754", "type": "Input", "portTypes": ["number"] }
  ],
  "connections": [
    { "from": "510", "to": "37", "fromPort": "o1", "toPort": "i3" },
    { "from": "495", "to": "37", "fromPort": "o1", "toPort": "i1" },
    { "from": "861", "to": "37", "fromPort": "o1", "toPort": "i2" },
    { "from": "996", "to": "495", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "495", "fromPort": "o1", "toPort": "i1" },
    { "from": "996", "to": "861", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "861", "fromPort": "o1", "toPort": "i1" },
    { "from": "865", "to": "996", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "865", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "510", "fromPort": "o1", "toPort": "i1" },
    { "from": "754", "to": "262", "fromPort": "o1", "toPort": "i1" }
  ]
})JSON";

// Input -> ResamplerHermite -> MovingSum(5) -> Output. Exercises the
// resampler multi-emit feeding a windowed-stateful aggregator that must
// consume each emitted tick rather than only the last one.
const char* kResamplerMovingSumJson = R"({
  "entryOperator": "input",
  "output": { "out": ["o1"] },
  "operators": [
    { "id": "input", "type": "Input", "portTypes": ["number"] },
    { "id": "rs",    "type": "ResamplerHermite", "interval": 1 },
    { "id": "msum",  "type": "MovingSum", "window_size": 5 },
    { "id": "out",   "type": "Output", "portTypes": ["number"] }
  ],
  "connections": [
    { "from": "input", "to": "rs",   "fromPort": "o1", "toPort": "i1" },
    { "from": "rs",    "to": "msum", "fromPort": "o1", "toPort": "i1" },
    { "from": "msum",  "to": "out",  "fromPort": "o1", "toPort": "i1" }
  ]
})";

// Input -> ResamplerHermite -> Difference -> Output. Single-output multi-emit
// where every emitted tick must reach the consumer; this is the same shape
// the original bollinger bug hid in.
const char* kResamplerDiffJson = R"({
  "entryOperator": "input",
  "output": { "out": ["o1"] },
  "operators": [
    { "id": "input", "type": "Input", "portTypes": ["number"] },
    { "id": "rs",    "type": "ResamplerHermite", "interval": 1 },
    { "id": "diff",  "type": "Difference" },
    { "id": "out",   "type": "Output", "portTypes": ["number"] }
  ],
  "connections": [
    { "from": "input", "to": "rs",   "fromPort": "o1", "toPort": "i1" },
    { "from": "rs",    "to": "diff", "fromPort": "o1", "toPort": "i1" },
    { "from": "diff",  "to": "out",  "fromPort": "o1", "toPort": "i1" }
  ]
})";

// Deterministic thinned-input generator mirroring the bollinger integration
// test's pattern: tick 0 always present; ticks 1..end-1 retained with ~50%
// probability via a seeded rand; tick end always present. The seed is fixed
// inside the SCENARIO so the input topology is reproducible regardless of
// when the test runs.
std::vector<rtbot::test::TickInput> thinned_quadratic_inputs(
    std::int64_t end, unsigned seed) {
  std::vector<rtbot::test::TickInput> out;
  std::srand(seed);
  out.push_back({0, {0.0}});
  for (std::int64_t i = 1; i < end; ++i) {
    if (std::rand() < (RAND_MAX / 2)) {
      const double v = static_cast<double>(i) * static_cast<double>(i);
      out.push_back({i, {v}});
    }
  }
  out.push_back({end, {static_cast<double>(end) * static_cast<double>(end)}});
  return out;
}

SCENARIO("framework parity: Bollinger Bands (ResamplerHermite, thinned input)",
         "[program][jit][framework_parity][resampler][bollinger]") {
  GIVEN("the bollinger pipeline driven by a thinned quadratic stream") {
    auto inputs = thinned_quadratic_inputs(1000, 42);
    rtbot::test::run_jit_vs_interpreter_parity(
        kBollingerBandsJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: Bollinger Bands (ResamplerConstant, thinned input)",
         "[program][jit][framework_parity][resampler]") {
  GIVEN("the bollinger pipeline using a constant resampler") {
    auto inputs = thinned_quadratic_inputs(1000, 42);
    rtbot::test::run_jit_vs_interpreter_parity(
        kBollingerConstantJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: ResamplerHermite -> MovingSum under thinned input",
         "[program][jit][framework_parity][resampler]") {
  GIVEN("a 5-point moving sum consuming resampled ticks from a thinned ramp") {
    auto inputs = thinned_quadratic_inputs(500, 42);
    rtbot::test::run_jit_vs_interpreter_parity(
        kResamplerMovingSumJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: ResamplerHermite -> Difference under thinned input",
         "[program][jit][framework_parity][resampler]") {
  GIVEN("a Difference op consuming resampled ticks from a thinned ramp") {
    auto inputs = thinned_quadratic_inputs(500, 42);
    rtbot::test::run_jit_vs_interpreter_parity(
        kResamplerDiffJson, inputs, send_scalar());
  }
}

SCENARIO("framework parity: RSI(14) integration program",
         "[program][jit][framework_parity][rsi]") {
  GIVEN("the integration_test_rsi_program 14-period RSI graph") {
    const std::vector<std::pair<std::int64_t, double>> prices = {
        {1, 54.8},   {2, 56.8},   {3, 57.85},  {4, 59.85},  {5, 60.57},
        {6, 61.1},   {7, 62.17},  {8, 60.6},   {9, 62.35},  {10, 62.15},
        {11, 62.35}, {12, 61.45}, {13, 62.8},  {14, 61.37}, {15, 62.5},
        {16, 62.57}, {17, 60.8},  {18, 59.37}, {19, 60.35}, {20, 62.35},
        {21, 62.17}, {22, 62.55}, {23, 64.55}, {24, 64.37}, {25, 65.3},
        {26, 64.42}, {27, 62.9},  {28, 61.6},  {29, 62.05}, {30, 60.05},
        {31, 59.7},  {32, 60.9},  {33, 60.25}, {34, 58.27}, {35, 58.7},
        {36, 57.72}, {37, 58.1},  {38, 58.2}};
    std::vector<rtbot::test::TickInput> inputs;
    inputs.reserve(prices.size());
    for (const auto& [t, v] : prices) {
      inputs.push_back({t, {v}});
    }
    rtbot::test::run_jit_vs_interpreter_parity(
        kRsi14Json, inputs, send_scalar());
  }
}
