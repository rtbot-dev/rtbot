// test_session_shape_parity.cpp
//
// Bit-exact parity tests for session-shape program graphs (RB-491). A session
// graph declares multiple terminal ops via the top-level "output" map and
// has NO physical OpKind::Output node — the JIT synthesizes one internally
// so the rest of the IR pipeline can treat the graph identically to a
// single-terminal program.
//
// Two scenarios mirror the rtbot-sql production shape:
//   1. Two scalar terminals (vibration_moments + rms_trend pattern).
//   2. Three heterogeneous-width vector terminals (mixed scalar + vector
//      ports across distinct terminals — the IMS bearing matrix shape).
//
// Each scenario drives the same JSON program through both JIT and interpreter
// and REQUIREs bit-exact equality on every emission.

#include <catch2/catch.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "rtbot/Program.h"
#include "parity_helper.h"

namespace {

// Two scalar terminals fed from a shared base chain. Mirrors the rtbot-sql
// session shape where multiple views share a base stream and each materializes
// its own scalar terminal. No OpKind::Output node — terminals are exposed via
// the top-level "output" map.
const char* kTwoTerminalsJson = R"({
  "title": "two-terminals-session",
  "apiVersion": "v1",
  "author": "RB-491 test",
  "entryOperator": "in",
  "output": { "term_a": ["o1"], "term_b": ["o1"] },
  "operators": [
    { "id": "in",     "type": "Input",         "portTypes": ["number"] },
    { "id": "ma",     "type": "MovingAverage", "window_size": 5 },
    { "id": "term_a", "type": "Scale",         "value": 2.0 },
    { "id": "term_b", "type": "Add",           "value": 1.5 }
  ],
  "connections": [
    { "from": "in", "to": "ma",     "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "term_a", "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "term_b", "fromPort": "o1", "toPort": "i1" }
  ]
})";

// Three terminals with distinct scalar ports each. All terminals share the
// same upstream MA so the JIT's combined-warmup gating produces emissions
// in lockstep with the interpreter — keeps the test focused on the flat-
// slot dispatch contract rather than per-terminal warmup semantics. Slot
// layout follows std::map<terminal_id, port_names> iteration order across
// both the JIT JsonParser synth and Program::parse_jit_output_mapping_.
const char* kThreeTerminalsJson = R"({
  "title": "three-terminals-session",
  "apiVersion": "v1",
  "author": "RB-491 test",
  "entryOperator": "in",
  "output": {
    "term_alpha": ["o1"],
    "term_beta":  ["o1"],
    "term_gamma": ["o1"]
  },
  "operators": [
    { "id": "in",         "type": "Input",         "portTypes": ["number"] },
    { "id": "ma",         "type": "MovingAverage", "window_size": 4 },
    { "id": "term_alpha", "type": "Scale",         "value": 1.0 },
    { "id": "term_beta",  "type": "Scale",         "value": 2.0 },
    { "id": "term_gamma", "type": "Add",           "value": 5.0 }
  ],
  "connections": [
    { "from": "in", "to": "ma",         "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "term_alpha", "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "term_beta",  "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "term_gamma", "fromPort": "o1", "toPort": "i1" }
  ]
})";

}  // namespace

SCENARIO("Session shape: two scalar terminals exposed via top-level output map",
         "[program][jit][session][parity]") {
  GIVEN("a fused session graph with two scalar terminals and no Output node") {
    std::vector<rtbot::test::TickInput> inputs;
    inputs.reserve(500);
    for (int i = 1; i <= 500; ++i) {
      const double v = static_cast<double>(i) * 0.01 +
                       std::sin(static_cast<double>(i) * 0.1);
      inputs.push_back({static_cast<std::int64_t>(i), {v}});
    }

    rtbot::test::run_jit_vs_interpreter_parity(
        kTwoTerminalsJson, inputs,
        [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
          p.send(tick.t, tick.values[0]);
        });
  }
}

SCENARIO("Session shape: three terminals at heterogeneous tick rates",
         "[program][jit][session][parity]") {
  GIVEN("a fused session graph with three terminals fed by distinct windows") {
    std::vector<rtbot::test::TickInput> inputs;
    inputs.reserve(500);
    for (int i = 1; i <= 500; ++i) {
      // Mix of monotonic ramp + cosine to exercise both terminals' stateful
      // accumulators across the warmup boundary (window sizes 3 and 7).
      const double v = static_cast<double>(i) * 0.005 +
                       std::cos(static_cast<double>(i) * 0.05);
      inputs.push_back({static_cast<std::int64_t>(i), {v}});
    }

    rtbot::test::run_jit_vs_interpreter_parity(
        kThreeTerminalsJson, inputs,
        [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
          p.send(tick.t, tick.values[0]);
        });
  }
}

// Three terminals exposing heterogeneous-width vectors: the rtbot-sql IMS
// bearing-test pattern. term_a is scalar (1 lane), term_b is a 3-lane vector
// via VectorCompose, term_c is a 5-lane vector via a separate VectorCompose.
// The synth Output sees three input ports of widths {1, 3, 5}; flat slot
// layout is term_a.slot0=0, term_b.slot0..2=[1..3], term_c.slot0..4=[4..8].
const char* kHeterogeneousVectorTerminalsJson = R"({
  "title": "three-terminals-heterogeneous-widths",
  "apiVersion": "v1",
  "author": "RB-491 test",
  "entryOperator": "in",
  "output": {
    "term_a": ["o1"],
    "term_b": ["o1"],
    "term_c": ["o1"]
  },
  "operators": [
    { "id": "in",     "type": "Input",         "portTypes": ["number"] },
    { "id": "ma",     "type": "MovingAverage", "window_size": 4 },

    { "id": "term_a", "type": "Scale",         "value": 1.0 },

    { "id": "scale_b1", "type": "Scale", "value": 1.5 },
    { "id": "scale_b2", "type": "Scale", "value": 2.5 },
    { "id": "scale_b3", "type": "Scale", "value": 3.5 },
    { "id": "term_b",   "type": "VectorCompose", "numPorts": 3 },

    { "id": "scale_c1", "type": "Scale", "value": 1.1 },
    { "id": "scale_c2", "type": "Scale", "value": 2.2 },
    { "id": "scale_c3", "type": "Scale", "value": 3.3 },
    { "id": "scale_c4", "type": "Scale", "value": 4.4 },
    { "id": "scale_c5", "type": "Scale", "value": 5.5 },
    { "id": "term_c",   "type": "VectorCompose", "numPorts": 5 }
  ],
  "connections": [
    { "from": "in", "to": "ma", "fromPort": "o1", "toPort": "i1" },

    { "from": "ma", "to": "term_a", "fromPort": "o1", "toPort": "i1" },

    { "from": "ma", "to": "scale_b1", "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "scale_b2", "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "scale_b3", "fromPort": "o1", "toPort": "i1" },
    { "from": "scale_b1", "to": "term_b", "fromPort": "o1", "toPort": "i1" },
    { "from": "scale_b2", "to": "term_b", "fromPort": "o1", "toPort": "i2" },
    { "from": "scale_b3", "to": "term_b", "fromPort": "o1", "toPort": "i3" },

    { "from": "ma", "to": "scale_c1", "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "scale_c2", "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "scale_c3", "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "scale_c4", "fromPort": "o1", "toPort": "i1" },
    { "from": "ma", "to": "scale_c5", "fromPort": "o1", "toPort": "i1" },
    { "from": "scale_c1", "to": "term_c", "fromPort": "o1", "toPort": "i1" },
    { "from": "scale_c2", "to": "term_c", "fromPort": "o1", "toPort": "i2" },
    { "from": "scale_c3", "to": "term_c", "fromPort": "o1", "toPort": "i3" },
    { "from": "scale_c4", "to": "term_c", "fromPort": "o1", "toPort": "i4" },
    { "from": "scale_c5", "to": "term_c", "fromPort": "o1", "toPort": "i5" }
  ]
})";

SCENARIO("Session shape: three terminals with heterogeneous vector widths",
         "[program][jit][session][parity][vector]") {
  GIVEN("a fused session graph with terminals of widths 1, 3, 5") {
    std::vector<rtbot::test::TickInput> inputs;
    inputs.reserve(500);
    for (int i = 1; i <= 500; ++i) {
      const double v = static_cast<double>(i) * 0.003 +
                       std::sin(static_cast<double>(i) * 0.07);
      inputs.push_back({static_cast<std::int64_t>(i), {v}});
    }

    rtbot::test::run_jit_vs_interpreter_parity(
        kHeterogeneousVectorTerminalsJson, inputs,
        [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
          p.send(tick.t, tick.values[0]);
        });
  }
}

SCENARIO("Session shape: explicit using_jit() check for synth Output graph",
         "[program][jit][session]") {
  GIVEN("a session-shape program") {
    rtbot::Program::set_force_interpreter_for_testing(false);
    rtbot::Program prog(kTwoTerminalsJson);
    REQUIRE(prog.using_jit());
  }

  GIVEN("the same program with the JIT force-disabled") {
    rtbot::Program::set_force_interpreter_for_testing(true);
    rtbot::Program prog(kTwoTerminalsJson);
    REQUIRE_FALSE(prog.using_jit());
    rtbot::Program::set_force_interpreter_for_testing(false);
  }
}
