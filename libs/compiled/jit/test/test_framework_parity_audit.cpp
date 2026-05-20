// test_framework_parity_audit.cpp
//
// Audit-driven parity SCENARIOs that systematically wrap the diverse JSON
// pipelines used across libs/{api,std,core,finance}/test and examples/data
// into per-emission bit-exact JIT-vs-framework comparisons via
// rtbot::test::run_jit_vs_interpreter_parity. Each SCENARIO is selected to
// represent a unique operator-mix signature not already pinned in
// test_framework_parity.cpp; collectively they constitute a regression guard
// over the program-bearing test corpus.

#include <catch2/catch.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "parity_helper.h"
#include "rtbot/Program.h"

namespace {

// ===========================================================================
// JSON pipelines under audit
// ===========================================================================

// Difference scalar (single op).
const char* kDifferenceJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "Difference", "id": "diff1"},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "diff1", "fromPort": "o1", "toPort": "i1"},
    {"from": "diff1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// ResamplerHermite standalone — single op upstream of Output.
const char* kResamplerHermiteAloneJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "ResamplerHermite", "id": "rs", "interval": 1},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "rs", "fromPort": "o1", "toPort": "i1"},
    {"from": "rs", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// ResamplerConstant standalone.
const char* kResamplerConstantAloneJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "ResamplerConstant", "id": "rs", "interval": 2},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "rs", "fromPort": "o1", "toPort": "i1"},
    {"from": "rs", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// Linear with feedback shape mirroring the RSI EMA half: ts -> Linear -> ts -> back.
// Standalone Linear with coefficients fed by a Scale to test linear arithmetic.
const char* kLinearChainJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "Scale", "id": "sc1", "value": 0.5},
    {"type": "Linear", "id": "lin1", "coefficients": [0.7, 0.3]},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "sc1", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "lin1", "fromPort": "o1", "toPort": "i1"},
    {"from": "sc1", "to": "lin1", "fromPort": "o1", "toPort": "i2"},
    {"from": "lin1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// CountNumber feeding comparators feeding constants — emulates the RSI
// dispatch control path in isolation. Tests that count + comparators +
// boolean constants stay in sync between modes.
const char* kCountDispatchJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "CountNumber", "id": "cnt"},
    {"type": "LessThan", "id": "lt", "value": 5},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "cnt", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "lt", "fromPort": "o1", "toPort": "i1"},
    {"from": "lt", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// Two-port Output collecting two filter branches — the parity helper sees
// ZERO drained outputs on the JIT side but 19 on the framework side. This
// flagged a real candidate JIT divergence around multi-port Output drains
// (see audit notes in test commit message).
const char* kCountDispatchTwoPortJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "CountNumber", "id": "cnt"},
    {"type": "LessThan", "id": "lt", "value": 5},
    {"type": "GreaterThan", "id": "gt", "value": 5},
    {"type": "Output", "id": "output1", "portTypes": ["number", "number"]}
  ],
  "connections": [
    {"from": "input1", "to": "cnt", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "lt", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "gt", "fromPort": "o1", "toPort": "i1"},
    {"from": "lt", "to": "output1", "fromPort": "o1", "toPort": "i1"},
    {"from": "gt", "to": "output1", "fromPort": "o1", "toPort": "i2"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1", "o2"] }
})";

// 3-port Output where every port's upstream filter fires on a disjoint
// range of the counter, so no two ports ever emit on the same tick. Stress
// test for the OR-of-port-flags commit gate.
const char* kCountDispatchThreePortAllAsymmetricJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "CountNumber", "id": "cnt"},
    {"type": "LessThan", "id": "lt", "value": 7},
    {"type": "GreaterThan", "id": "gt", "value": 14},
    {"type": "EqualTo", "id": "eq", "value": 10},
    {"type": "Output", "id": "output1",
      "portTypes": ["number", "number", "number"]}
  ],
  "connections": [
    {"from": "input1", "to": "cnt", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "lt", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "gt", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "eq", "fromPort": "o1", "toPort": "i1"},
    {"from": "lt", "to": "output1", "fromPort": "o1", "toPort": "i1"},
    {"from": "eq", "to": "output1", "fromPort": "o1", "toPort": "i2"},
    {"from": "gt", "to": "output1", "fromPort": "o1", "toPort": "i3"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1", "o2", "o3"] }
})";

// Mixed: 2 symmetric (unconditional) ports + 1 asymmetric (filter-gated).
// Pins the case where one port carries a conditional emit_flag while the
// others are unconditional — without the NaN-mask of just the conditional
// port the JIT would either suppress all ports (old combined_flag) or
// poison every tick's record with stale data.
const char* kMixedSymAsymThreePortJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "CountNumber", "id": "cnt"},
    {"type": "Identity", "id": "id1"},
    {"type": "Scale", "id": "sc", "value": 2.0},
    {"type": "GreaterThan", "id": "gt", "value": 5},
    {"type": "Output", "id": "output1",
      "portTypes": ["number", "number", "number"]}
  ],
  "connections": [
    {"from": "input1", "to": "cnt", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "id1", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "sc", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "gt", "fromPort": "o1", "toPort": "i1"},
    {"from": "id1", "to": "output1", "fromPort": "o1", "toPort": "i1"},
    {"from": "sc", "to": "output1", "fromPort": "o1", "toPort": "i2"},
    {"from": "gt", "to": "output1", "fromPort": "o1", "toPort": "i3"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1", "o2", "o3"] }
})";

// 2-port Output where one port is rarely active: EqualTo fires only on the
// counter values matching its target. The other port emits unconditionally.
// Boundary case where most ticks have a dead port — verifies the commit
// gate stays correct when activity is heavily skewed.
const char* kRarelyActivePortJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "CountNumber", "id": "cnt"},
    {"type": "Identity", "id": "id1"},
    {"type": "EqualTo", "id": "eq", "value": 17},
    {"type": "Output", "id": "output1", "portTypes": ["number", "number"]}
  ],
  "connections": [
    {"from": "input1", "to": "cnt", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "id1", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "eq", "fromPort": "o1", "toPort": "i1"},
    {"from": "id1", "to": "output1", "fromPort": "o1", "toPort": "i1"},
    {"from": "eq", "to": "output1", "fromPort": "o1", "toPort": "i2"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1", "o2"] }
})";

// Join + Identity + Scale (mirrors examples/data/program-test-3.json shape
// using current type aliases). Identity fans into two Join ports; Join
// emits a pair that Scale chains process. Output collects two ports.
const char* kJoinScaleJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "Identity", "id": "id1"},
    {"type": "Join", "id": "join1", "portTypes": ["number", "number"]},
    {"type": "Scale", "id": "sc1", "value": 2.0},
    {"type": "Scale", "id": "sc2", "value": 3.0},
    {"type": "Output", "id": "output1", "portTypes": ["number", "number"]}
  ],
  "connections": [
    {"from": "input1", "to": "id1", "fromPort": "o1", "toPort": "i1"},
    {"from": "id1", "to": "join1", "fromPort": "o1", "toPort": "i1"},
    {"from": "id1", "to": "join1", "fromPort": "o1", "toPort": "i2"},
    {"from": "join1", "to": "sc1", "fromPort": "o1", "toPort": "i1"},
    {"from": "join1", "to": "sc2", "fromPort": "o2", "toPort": "i1"},
    {"from": "sc1", "to": "output1", "fromPort": "o1", "toPort": "i1"},
    {"from": "sc2", "to": "output1", "fromPort": "o1", "toPort": "i2"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1", "o2"] }
})";

// Demultiplexer with two control ports driven by comparators (the RSI
// dispatch pattern in isolation, without the linear EMA tail).
const char* kDemuxComparatorsJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "CountNumber", "id": "cnt"},
    {"type": "GreaterThan", "id": "gt", "value": 0.0},
    {"type": "LessThan", "id": "lt", "value": 0.0},
    {"type": "ConstantNumberToBoolean", "id": "cb_gt_true", "value": true},
    {"type": "ConstantNumberToBoolean", "id": "cb_gt_false", "value": false},
    {"type": "ConstantNumberToBoolean", "id": "cb_lt_true", "value": true},
    {"type": "ConstantNumberToBoolean", "id": "cb_lt_false", "value": false},
    {"type": "Demultiplexer", "id": "dm", "numPorts": 2},
    {"type": "Output", "id": "output1", "portTypes": ["number", "number"]}
  ],
  "connections": [
    {"from": "input1", "to": "cnt", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "dm", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "gt", "fromPort": "o1", "toPort": "i1"},
    {"from": "cnt", "to": "lt", "fromPort": "o1", "toPort": "i1"},
    {"from": "gt", "to": "cb_gt_true", "fromPort": "o1", "toPort": "i1"},
    {"from": "gt", "to": "cb_gt_false", "fromPort": "o1", "toPort": "i1"},
    {"from": "lt", "to": "cb_lt_true", "fromPort": "o1", "toPort": "i1"},
    {"from": "lt", "to": "cb_lt_false", "fromPort": "o1", "toPort": "i1"},
    {"from": "cb_gt_true", "to": "dm", "fromPort": "o1", "toPort": "c1"},
    {"from": "cb_lt_false", "to": "dm", "fromPort": "o1", "toPort": "c1"},
    {"from": "cb_gt_false", "to": "dm", "fromPort": "o1", "toPort": "c2"},
    {"from": "cb_lt_true", "to": "dm", "fromPort": "o1", "toPort": "c2"},
    {"from": "dm", "to": "output1", "fromPort": "o1", "toPort": "i1"},
    {"from": "dm", "to": "output1", "fromPort": "o2", "toPort": "i2"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1", "o2"] }
})";

// Pipeline with MA-only inside (test_program.cpp lines 351-476 shape).
// Single-op pipeline with explicit Input op inside; tests reset across
// segments via key flips.
const char* kPipelineMaResetJson = R"({
  "operators": [
    {"type": "Input", "id": "input", "portTypes": ["number", "number"]},
    {
      "type": "Pipeline",
      "id": "pipeline",
      "input_port_types": ["number"],
      "output_port_types": ["number"],
      "operators": [
        {"type": "Input", "id": "pinput", "portTypes": ["number"]},
        {"type": "MovingAverage", "id": "ma", "window_size": 3}
      ],
      "connections": [
        {"from": "pinput", "to": "ma", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "pinput",
      "outputMappings": { "ma": {"o1": "o1"} }
    },
    {"type": "Output", "id": "output", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input", "to": "pipeline", "fromPort": "o1", "toPort": "i1"},
    {"from": "input", "to": "pipeline", "fromPort": "o2", "toPort": "c1"},
    {"from": "pipeline", "to": "output", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input",
  "output": { "output": ["o1"] }
})";

// Pipeline MA(3) -> MA(2) — the serialization/reset shape from
// test_program.cpp lines 768-1013.
const char* kPipelineMaMaJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
    {
      "type": "Pipeline",
      "id": "pipeline1",
      "input_port_types": ["number"],
      "output_port_types": ["number"],
      "operators": [
        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
        {"type": "MovingAverage", "id": "ma2", "window_size": 2}
      ],
      "connections": [
        {"from": "ma1", "to": "ma2", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "ma1",
      "outputMappings": { "ma2": {"o1": "o1"} }
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

// Pipeline MA(2) -> Scale -> StdDev(3) — the deeper 3-op pipeline shape used
// by the nested-prototype test (lines 1297-1353).
const char* kPipelineMaScaleStdJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
    {
      "type": "Pipeline",
      "id": "pipeline1",
      "input_port_types": ["number"],
      "output_port_types": ["number"],
      "operators": [
        {"type": "MovingAverage", "id": "ma", "window_size": 2},
        {"type": "Scale", "id": "scale", "value": 2.0},
        {"type": "StandardDeviation", "id": "std", "window_size": 3}
      ],
      "connections": [
        {"from": "ma", "to": "scale", "fromPort": "o1", "toPort": "i1"},
        {"from": "scale", "to": "std", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "ma",
      "outputMappings": { "std": {"o1": "o1"} }
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

// TriggerSet MA(3) -> MA(2) — the trigger_set serialization shape from
// test_trigger_set.cpp lines 411-498.
const char* kTriggerSetMaMaJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {
      "type": "TriggerSet",
      "id": "ts1",
      "input_port_type": "number",
      "output_port_type": "number",
      "operators": [
        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
        {"type": "MovingAverage", "id": "ma2", "window_size": 2}
      ],
      "connections": [
        {"from": "ma1", "to": "ma2", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "ma1",
      "outputOperator": {"id": "ma2", "port": "o1"}
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

// Prototype expanding to MovingAverage + Scale (test_program.cpp lines
// 1015-1093). Tests parameter substitution and prototype expansion before
// JIT/interpreter dispatch.
const char* kPrototypeMaScaleJson = R"({
  "prototypes": {
    "adjustableMA": {
      "parameters": [
        {"name": "window", "type": "number"},
        {"name": "scale", "type": "number", "default": 1.0}
      ],
      "operators": [
        {"type": "MovingAverage", "id": "ma", "window_size": "${window}"},
        {"type": "Scale", "id": "scale", "value": "${scale}"}
      ],
      "connections": [
        {"from": "ma", "to": "scale"}
      ],
      "entry": {"operator": "ma"},
      "output": {"operator": "scale"}
    }
  },
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"id": "ma_instance", "prototype": "adjustableMA", "parameters": {"window": 4, "scale": 2.5}},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "ma_instance"},
    {"from": "ma_instance", "to": "output1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// Nested prototypes: dualMA -> simpleMA twice (test_program.cpp 1499-1569).
const char* kNestedPrototypeJson = R"({
  "prototypes": {
    "simpleMA": {
      "parameters": [
        {"name": "window", "type": "number"}
      ],
      "operators": [
        {"type": "MovingAverage", "id": "ma", "window_size": "${window}"}
      ],
      "connections": [],
      "entry": {"operator": "ma"},
      "output": {"operator": "ma"}
    },
    "dualMA": {
      "parameters": [
        {"name": "window1", "type": "number"},
        {"name": "window2", "type": "number"}
      ],
      "operators": [
        {"id": "ma1", "prototype": "simpleMA", "parameters": {"window": "${window1}"}},
        {"id": "ma2", "prototype": "simpleMA", "parameters": {"window": "${window2}"}}
      ],
      "connections": [
        {"from": "ma1::ma", "to": "ma2::ma"}
      ],
      "entry": {"operator": "ma1::ma"},
      "output": {"operator": "ma2::ma"}
    }
  },
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"id": "nested_ma", "prototype": "dualMA", "parameters": {"window1": 3, "window2": 4}},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "nested_ma::ma1::ma"},
    {"from": "nested_ma::ma2::ma", "to": "output1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// Prototype with Pipeline (MA+StdDev) inside — test_program.cpp 1164-1257.
const char* kPrototypePipelineMaStdJson = R"({
  "prototypes": {
    "complexProcessor": {
      "parameters": [
        {"name": "ma_window", "type": "number"},
        {"name": "std_window", "type": "number", "default": 3}
      ],
      "operators": [
        {
          "type": "Pipeline",
          "id": "pipeline",
          "input_port_types": ["number"],
          "output_port_types": ["number"],
          "operators": [
            {"type": "MovingAverage", "id": "ma", "window_size": "${ma_window}"},
            {"type": "StandardDeviation", "id": "std", "window_size": "${std_window}"}
          ],
          "connections": [
            {"from": "ma", "to": "std", "fromPort": "o1", "toPort": "i1"}
          ],
          "entryOperator": "ma",
          "outputMappings": { "std": {"o1": "o1"} }
        }
      ],
      "connections": [],
      "entry": {"operator": "pipeline"},
      "output": {"operator": "pipeline"}
    }
  },
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
    {"id": "proc1", "prototype": "complexProcessor", "parameters": {"ma_window": 3}},
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "proc1", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "proc1", "fromPort": "o2", "toPort": "c1"},
    {"from": "proc1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
})";

// Variable + TimeShift + Linear feedback loop — the RSI EMA half in
// isolation. Captures the "ts -> linear -> ts -> back to linear" feedback
// shape independent of the surrounding dispatch lattice.
const char* kEmaFeedbackJson = R"({
  "operators": [
    {"id": "input", "type": "Input", "portTypes": ["number"]},
    {"id": "var", "type": "Variable", "default_value": 0.0},
    {"id": "ts1", "type": "TimeShift", "shift": 1},
    {"id": "ts2", "type": "TimeShift", "shift": 1},
    {"id": "lin", "type": "Linear", "coefficients": [0.9, 0.1]},
    {"id": "output", "type": "Output", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input", "to": "var", "fromPort": "o1", "toPort": "i1"},
    {"from": "var", "to": "ts1", "fromPort": "o1", "toPort": "i1"},
    {"from": "ts1", "to": "lin", "fromPort": "o1", "toPort": "i1"},
    {"from": "lin", "to": "ts2", "fromPort": "o1", "toPort": "i1"},
    {"from": "ts2", "to": "lin", "fromPort": "o1", "toPort": "i2"},
    {"from": "lin", "to": "output", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input",
  "output": { "output": ["o1"] }
})";

// Bollinger multi-output via examples/data/program-test-6.json (already
// covered) — but with smaller window for fast bring-up of a JIT-engaged
// 3-port output.
const char* kMultiPortOutputJson = R"({
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number"]},
    {"type": "MovingAverage", "id": "ma", "window_size": 4},
    {"type": "StandardDeviation", "id": "std", "window_size": 4},
    {"type": "Scale", "id": "sc", "value": 2.0},
    {"type": "Addition", "id": "upper"},
    {"type": "Subtraction", "id": "lower"},
    {"type": "Output", "id": "out", "portTypes": ["number", "number", "number"]}
  ],
  "connections": [
    {"from": "input1", "to": "ma", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "std", "fromPort": "o1", "toPort": "i1"},
    {"from": "std", "to": "sc", "fromPort": "o1", "toPort": "i1"},
    {"from": "ma", "to": "upper", "fromPort": "o1", "toPort": "i1"},
    {"from": "sc", "to": "upper", "fromPort": "o1", "toPort": "i2"},
    {"from": "ma", "to": "lower", "fromPort": "o1", "toPort": "i1"},
    {"from": "sc", "to": "lower", "fromPort": "o1", "toPort": "i2"},
    {"from": "lower", "to": "out", "fromPort": "o1", "toPort": "i1"},
    {"from": "upper", "to": "out", "fromPort": "o1", "toPort": "i2"},
    {"from": "ma", "to": "out", "fromPort": "o1", "toPort": "i3"}
  ],
  "entryOperator": "input1",
  "output": { "out": ["o1", "o2", "o3"] }
})";

// Hourly cutoff/resample/scale pipeline (test_trigger_set.cpp 212-407),
// driven at the 5000-ms cadence the original test uses but condensed.
const char* kHourlyTriggerSetJson = R"({
  "apiVersion": "v1",
  "operators": [
    {"id": "input", "type": "Input", "portTypes": ["number", "number"]},
    {"id": "hi_cutoff", "type": "LessThanOrEqualToReplace", "value": 0.5, "replaceBy": 0.0},
    {"id": "lo_cutoff", "type": "LessThanOrEqualToReplace", "value": 0.5, "replaceBy": 0.0},
    {"id": "hires", "type": "ResamplerConstant", "interval": 5000},
    {"id": "lores", "type": "ResamplerConstant", "interval": 5000},
    {"id": "power1", "type": "Scale", "value": 220},
    {"id": "power2", "type": "Scale", "value": 220},
    {"id": "total", "type": "Addition"},
    {
      "id": "hourly",
      "type": "TriggerSet",
      "input_port_type": "number",
      "output_port_type": "number",
      "operators": [
        {"id": "trapezoid", "type": "MovingAverage", "window_size": 2},
        {"id": "wh", "type": "Scale", "value": 0.00138888888},
        {"id": "ms", "type": "MovingSum", "window_size": 5}
      ],
      "connections": [
        {"from": "trapezoid", "to": "wh"},
        {"from": "wh", "to": "ms"}
      ],
      "entryOperator": "trapezoid",
      "outputOperator": {"id": "ms", "port": "o1"}
    },
    {"id": "output", "type": "Output", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input", "to": "hi_cutoff", "fromPort": "o1", "toPort": "i1"},
    {"from": "input", "to": "lo_cutoff", "fromPort": "o2", "toPort": "i1"},
    {"from": "hi_cutoff", "to": "hires"},
    {"from": "lo_cutoff", "to": "lores"},
    {"from": "hires", "to": "power1"},
    {"from": "lores", "to": "power2"},
    {"from": "power1", "to": "total", "fromPort": "o1", "toPort": "i1"},
    {"from": "power2", "to": "total", "fromPort": "o1", "toPort": "i2"},
    {"from": "total", "to": "hourly"},
    {"from": "hourly", "to": "output", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input",
  "output": { "output": ["o1"] }
})";

// ===========================================================================
// Deterministic input generators
// ===========================================================================

std::vector<rtbot::test::TickInput> ramp(int count, double base, double step,
                                         std::int64_t t0 = 1) {
  std::vector<rtbot::test::TickInput> out;
  out.reserve(count);
  for (int i = 0; i < count; ++i) {
    out.push_back({static_cast<std::int64_t>(t0 + i),
                   {base + step * static_cast<double>(i)}});
  }
  return out;
}

std::vector<rtbot::test::TickInput> sine(int count, double amp, double freq) {
  std::vector<rtbot::test::TickInput> out;
  out.reserve(count);
  for (int i = 1; i <= count; ++i) {
    out.push_back({static_cast<std::int64_t>(i),
                   {amp * std::sin(freq * static_cast<double>(i))}});
  }
  return out;
}

std::vector<rtbot::test::TickInput> random_walk(int count, unsigned seed) {
  std::vector<rtbot::test::TickInput> out;
  out.reserve(count);
  std::srand(seed);
  double v = 0.0;
  for (int i = 1; i <= count; ++i) {
    v += (std::rand() & 0xFFFF) / 65535.0 - 0.5;
    out.push_back({static_cast<std::int64_t>(i), {v}});
  }
  return out;
}

auto send_scalar() {
  return [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
    p.send(tick.t, tick.values[0]);
  };
}

auto send_two_ports() {
  return [](rtbot::Program& p, const rtbot::test::TickInput& tick) {
    p.send(tick.t, tick.values[0], "i1");
    p.send(tick.t, tick.values[1], "i2");
  };
}

// Driver for Pipeline tests: data on i1, key on i2. Pre-built keyed inputs
// that switch the key partway through to force segment flushes.
std::vector<rtbot::test::TickInput> keyed_segments(
    int count_per_segment, int n_segments, double base, double step) {
  std::vector<rtbot::test::TickInput> out;
  std::int64_t t = 1;
  for (int s = 0; s < n_segments; ++s) {
    const double key = static_cast<double>(s + 1);
    for (int i = 0; i < count_per_segment; ++i) {
      out.push_back({t, {base + step * static_cast<double>(t), key}});
      ++t;
    }
  }
  return out;
}

// Driver for hourly TriggerSet: three numeric inputs (hi, lo, unused).
std::vector<rtbot::test::TickInput> power_consumption_stream(int hours,
                                                             int steps_per_hour) {
  std::vector<rtbot::test::TickInput> out;
  std::int64_t t = 5000;
  for (int h = 1; h <= hours; ++h) {
    const double hi = (h % 2 == 1) ? 30.23 : 0.01;
    const double lo = (h % 2 == 1) ? 10.58 : 0.01;
    for (int i = 0; i < steps_per_hour; ++i) {
      out.push_back({t, {hi, lo}});
      t += 5000;
    }
  }
  return out;
}

}  // namespace

// ===========================================================================
// SCENARIOs
// ===========================================================================

SCENARIO("audit parity: Difference scalar",
         "[program_audit][parity][difference]") {
  GIVEN("a Difference op driven by a ramp") {
    auto inputs = ramp(64, 0.0, 1.5);
    rtbot::test::run_jit_vs_interpreter_parity(
        kDifferenceJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: ResamplerHermite standalone on ramp input",
         "[program_audit][parity][resampler]") {
  GIVEN("a Hermite resampler at interval=1 driven by a unit ramp") {
    auto inputs = ramp(64, 0.0, 1.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kResamplerHermiteAloneJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: ResamplerConstant standalone on sparse input",
         "[program_audit][parity][resampler]") {
  GIVEN("a constant resampler at interval=2 driven by a stride-3 ramp") {
    std::vector<rtbot::test::TickInput> inputs;
    for (int i = 1; i <= 64; ++i) {
      inputs.push_back({static_cast<std::int64_t>(3 * i),
                        {static_cast<double>(i)}});
    }
    rtbot::test::run_jit_vs_interpreter_parity(
        kResamplerConstantAloneJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: Linear with Scale fan-in",
         "[program_audit][parity][linear]") {
  GIVEN("a Linear op fed by both a Scale branch and the input") {
    auto inputs = ramp(80, 0.0, 0.5);
    rtbot::test::run_jit_vs_interpreter_parity(
        kLinearChainJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: CountNumber + LessThan filter",
         "[program_audit][parity][count]") {
  GIVEN("a counter gated by LessThan filter on a single output port") {
    auto inputs = ramp(20, 10.0, 0.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kCountDispatchJson, inputs, send_scalar());
  }
}

// Two-port asymmetric Output: each port's upstream filter fires on disjoint
// tick ranges. Pins the per-port emit_flag propagation through the JIT's
// Output writer (NaN-mask inactive slots, OR-of-port-flags as commit gate)
// so a regression that re-introduces AND-of-all-flags is caught.
SCENARIO("audit parity: CountNumber + comparators two-port Output",
         "[program_audit][parity][count]") {
  GIVEN("a counter feeding two comparators emitting on two output ports") {
    auto inputs = ramp(20, 10.0, 0.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kCountDispatchTwoPortJson, inputs, send_scalar());
  }
}

// Regression guard: every port asymmetric. Three filter-gated branches feed
// three Output ports on disjoint count ranges (<7, ==10, >14). Any port
// that ANDs its emit_flag with another's flag would suppress the whole
// record on every tick.
SCENARIO("audit parity: three-port Output, all asymmetric branches",
         "[program_audit][parity][multi_output]") {
  GIVEN("three comparator branches with disjoint activation ranges") {
    auto inputs = ramp(24, 0.0, 1.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kCountDispatchThreePortAllAsymmetricJson, inputs, send_scalar());
  }
}

// Regression guard: mixed symmetric + asymmetric. Two ports emit on every
// tick (Identity, Scale), one port emits only when count>5. Catches the
// case where the conditional flag must NaN-mask only its own port without
// disturbing the always-live ports' values.
SCENARIO("audit parity: three-port Output, mixed symmetric and asymmetric",
         "[program_audit][parity][multi_output]") {
  GIVEN("two unconditional ports plus one comparator-gated port") {
    auto inputs = ramp(24, 0.0, 0.5);
    rtbot::test::run_jit_vs_interpreter_parity(
        kMixedSymAsymThreePortJson, inputs, send_scalar());
  }
}

// Regression guard: one port active on a tiny minority of ticks (only when
// count==17 across 40 ramp samples). Verifies the commit gate stays
// correct when the OR-of-port-flags reduces to the rarely-active port for
// most ticks.
SCENARIO("audit parity: two-port Output with rarely active port",
         "[program_audit][parity][multi_output]") {
  GIVEN("one unconditional port plus one port firing on a single tick") {
    auto inputs = ramp(40, 1.0, 1.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kRarelyActivePortJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: Identity -> Join -> Scale fan-out",
         "[program_audit][parity][join]") {
  GIVEN("a Join that merges two copies of Identity, then Scales each port") {
    auto inputs = ramp(64, 0.0, 0.7);
    rtbot::test::run_jit_vs_interpreter_parity(
        kJoinScaleJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: Demultiplexer with comparator control",
         "[program_audit][parity][demultiplexer]") {
  GIVEN("a demux gated by GreaterThan/LessThan boolean constants") {
    auto inputs = sine(120, 5.0, 0.17);
    rtbot::test::run_jit_vs_interpreter_parity(
        kDemuxComparatorsJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: Pipeline with MA-only and segment resets",
         "[program_audit][parity][pipeline]") {
  GIVEN("a Pipeline holding only MA(3), driven across multiple key segments") {
    auto inputs = keyed_segments(5, 4, 0.0, 1.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kPipelineMaResetJson, inputs, send_two_ports());
  }
}

SCENARIO("audit parity: Pipeline MA(3) -> MA(2) chained",
         "[program_audit][parity][pipeline]") {
  GIVEN("a two-stage MA pipeline driven across segments") {
    auto inputs = keyed_segments(6, 3, 10.0, 5.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kPipelineMaMaJson, inputs, send_two_ports());
  }
}

SCENARIO("audit parity: Pipeline MA -> Scale -> StdDev",
         "[program_audit][parity][pipeline]") {
  GIVEN("a 3-stage pipeline (MA(2), Scale, StdDev(3)) across segments") {
    auto inputs = keyed_segments(6, 3, 1.0, 4.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kPipelineMaScaleStdJson, inputs, send_two_ports());
  }
}

SCENARIO("audit parity: TriggerSet MA(3) -> MA(2)",
         "[program_audit][parity][trigger_set]") {
  GIVEN("a TriggerSet holding two chained moving averages") {
    auto inputs = ramp(20, 0.0, 5.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kTriggerSetMaMaJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: Prototype expanding to MovingAverage + Scale",
         "[program_audit][parity][prototype]") {
  GIVEN("a parameterized prototype instance driven by a ramp") {
    auto inputs = ramp(64, 0.0, 1.25);
    rtbot::test::run_jit_vs_interpreter_parity(
        kPrototypeMaScaleJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: Nested prototypes (dualMA -> simpleMA x2)",
         "[program_audit][parity][prototype]") {
  GIVEN("a dualMA prototype expanding to nested simpleMA prototypes") {
    auto inputs = ramp(48, 1.0, 0.8);
    rtbot::test::run_jit_vs_interpreter_parity(
        kNestedPrototypeJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: Prototype wrapping Pipeline(MA + StdDev)",
         "[program_audit][parity][prototype][pipeline]") {
  GIVEN("a prototype that exposes a Pipeline with control-port plumbing") {
    auto inputs = keyed_segments(6, 3, 0.0, 3.0);
    rtbot::test::run_jit_vs_interpreter_parity(
        kPrototypePipelineMaStdJson, inputs, send_two_ports());
  }
}

SCENARIO("audit parity: Variable + TimeShift + Linear feedback loop",
         "[program_audit][parity][rsi]") {
  GIVEN("the RSI EMA half in isolation, driven by a random walk") {
    auto inputs = random_walk(80, 7);
    rtbot::test::run_jit_vs_interpreter_parity(
        kEmaFeedbackJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: Bollinger-shaped multi-port Output",
         "[program_audit][parity][multi_output]") {
  GIVEN("a 3-port Output collecting upper/lower/MA Bollinger components") {
    auto inputs = sine(120, 3.0, 0.09);
    rtbot::test::run_jit_vs_interpreter_parity(
        kMultiPortOutputJson, inputs, send_scalar());
  }
}

SCENARIO("audit parity: hourly TriggerSet with cutoffs and resampler",
         "[program_audit][parity][trigger_set]") {
  GIVEN("the power-consumption monitor at 5s cadence for ~3 hours") {
    auto inputs = power_consumption_stream(3, 6);
    rtbot::test::run_jit_vs_interpreter_parity(
        kHourlyTriggerSetJson, inputs, send_two_ports());
  }
}
