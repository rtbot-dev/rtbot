// test_trivial_ops_parity.cpp
//
// Bit-exact parity tests for the trivial-tier JIT IR emitters added in RB-491:
//   Identity, Constant (number), BooleanToNumber, TimeShift, TimestampExtract,
//   LessThanOrEqualToReplace.
//
// Each test drives the same input stream through:
//   - A hand-built FE interpreter pipeline (factory functions + Collector sink)
//   - A JIT-compiled program produced by JitCompiler::compile(json)
// then asserts the emitted (time, value) pairs are bit-exact equal.

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/std/BooleanToNumber.h"
#include "rtbot/std/Constant.h"
#include "rtbot/std/Identity.h"
#include "rtbot/std/Replace.h"
#include "rtbot/std/TimeShift.h"
#include "rtbot/std/TimestampExtract.h"
#include "rtbot/compiled/jit/JitCompiler.h"

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

struct Sample {
  std::int64_t t;
  double v;
};

struct Emit {
  std::int64_t t;
  double v;
};

// Drive the JIT program with a sequence of (t, v) samples and collect emits.
std::vector<Emit> run_jit(const std::string& json,
                          const std::vector<Sample>& inputs) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  for (const auto& s : inputs) prog->receive(s.t, s.v);
  std::vector<Emit> out;
  for (const auto& r : prog->collect_outputs()) {
    REQUIRE(r.values.size() == 1);
    out.push_back({r.time, r.values[0]});
  }
  return out;
}

// Drive an FE operator chain that has a single output port. The caller wires
// `op` to a number collector before invoking. After receive_data + execute,
// drains the collector queue into Emit records.
std::vector<Emit> drain_collector(rtbot::Collector& sink) {
  std::vector<Emit> out;
  auto& q = sink.get_data_queue(0);
  for (auto& msg_ptr : q) {
    const auto* msg = static_cast<const rtbot::Message<rtbot::NumberData>*>(msg_ptr.get());
    out.push_back({msg->time, msg->data.value});
  }
  q.clear();
  return out;
}

void require_parity(const std::vector<Emit>& jit_out,
                    const std::vector<Emit>& fe_out) {
  REQUIRE(jit_out.size() == fe_out.size());
  for (std::size_t i = 0; i < jit_out.size(); ++i) {
    INFO("emit index " << i
         << " jit=(" << jit_out[i].t << ", " << jit_out[i].v << ")"
         << " fe=("  << fe_out[i].t  << ", " << fe_out[i].v  << ")");
    REQUIRE(jit_out[i].t == fe_out[i].t);
    REQUIRE(dbits(jit_out[i].v) == dbits(fe_out[i].v));
  }
}

// Build a 100-sample input stream with a mix of normal, NaN, and Inf values.
std::vector<Sample> make_inputs() {
  std::vector<Sample> inputs;
  inputs.reserve(100);
  std::mt19937_64 rng(0xCAFED00DULL);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  for (std::int64_t i = 1; i <= 100; ++i) {
    double v = dist(rng);
    if (i == 17) v = std::numeric_limits<double>::quiet_NaN();
    if (i == 42) v = std::numeric_limits<double>::infinity();
    if (i == 73) v = -std::numeric_limits<double>::infinity();
    inputs.push_back({i, v});
  }
  return inputs;
}

}  // namespace

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------
SCENARIO("JIT Identity matches FE interpreter bit-exactly", "[trivial][identity]") {
  const char* kJson = R"({
    "title": "Identity parity",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input",    "portTypes": ["number"] },
      { "id": "id",  "type": "Identity" },
      { "id": "out", "type": "Output",   "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in", "to": "id",  "fromPort": "o1", "toPort": "i1" },
      { "from": "id", "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  auto inputs = make_inputs();

  auto jit_out = run_jit(kJson, inputs);

  // FE pipeline.
  auto op   = rtbot::make_identity("id");
  auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  op->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
  }
  op->execute();
  auto fe_out = drain_collector(*sink);

  require_parity(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Constant
// ---------------------------------------------------------------------------
SCENARIO("JIT Constant matches FE interpreter bit-exactly", "[trivial][constant]") {
  const char* kJson = R"({
    "title": "Constant parity",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input",          "portTypes": ["number"] },
      { "id": "c",   "type": "ConstantNumber", "value": 7.25 },
      { "id": "out", "type": "Output",         "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in", "to": "c",   "fromPort": "o1", "toPort": "i1" },
      { "from": "c",  "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  auto inputs = make_inputs();

  auto jit_out = run_jit(kJson, inputs);

  auto op   = rtbot::make_constant_number("c", 7.25);
  auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  op->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
  }
  op->execute();
  auto fe_out = drain_collector(*sink);

  require_parity(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// BooleanToNumber
// ---------------------------------------------------------------------------
//
// At the JIT level all values are doubles, so the JIT pipeline declares the
// upstream wire as "number" and the operator behaves as a passthrough. The FE
// reference must use the actual BooleanToNumber operator with BooleanData
// inputs. We feed both sides equivalent 0.0/1.0 inputs (FE uses true/false).
SCENARIO("JIT BooleanToNumber matches FE interpreter bit-exactly", "[trivial][booleantonumber]") {
  // For the JIT: declare port as number. Upstream emits 0.0/1.0 doubles.
  const char* kJson = R"({
    "title": "BooleanToNumber parity",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input",            "portTypes": ["number"] },
      { "id": "b2n", "type": "BooleanToNumber" },
      { "id": "out", "type": "Output",           "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in",  "to": "b2n", "fromPort": "o1", "toPort": "i1" },
      { "from": "b2n", "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  // 100 samples alternating 0.0/1.0 (representing false/true).
  std::vector<Sample> inputs;
  for (std::int64_t i = 1; i <= 100; ++i) {
    inputs.push_back({i, (i % 2 == 0) ? 1.0 : 0.0});
  }

  auto jit_out = run_jit(kJson, inputs);

  // FE reference: drive an actual BooleanToNumber op with BooleanData inputs.
  auto op   = rtbot::make_boolean_to_number("b2n");
  auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  op->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    bool b = (s.v != 0.0);
    op->receive_data(rtbot::create_message<rtbot::BooleanData>(s.t, rtbot::BooleanData{b}), 0);
  }
  op->execute();
  auto fe_out = drain_collector(*sink);

  require_parity(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// TimeShift
// ---------------------------------------------------------------------------
SCENARIO("JIT TimeShift matches FE interpreter for non-negative shifted times",
         "[trivial][timeshift]") {
  // Positive shift — no FE throw on any sample because all (t + 5) >= 0.
  const char* kJson = R"JSON({
    "title": "TimeShift parity positive shift",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input",     "portTypes": ["number"] },
      { "id": "ts",  "type": "TimeShift", "shift": 5 },
      { "id": "out", "type": "Output",    "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in", "to": "ts",  "fromPort": "o1", "toPort": "i1" },
      { "from": "ts", "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })JSON";

  // Use only finite, non-NaN inputs (the FE TimeShift doesn't filter NaN/Inf;
  // it just adjusts the timestamp regardless of value).
  std::vector<Sample> inputs;
  std::mt19937_64 rng(0xC0FFEEULL);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  for (std::int64_t i = 1; i <= 100; ++i) {
    inputs.push_back({i, dist(rng)});
  }

  auto jit_out = run_jit(kJson, inputs);

  auto op   = rtbot::make_time_shift("ts", 5);
  auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  op->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
  }
  op->execute();
  auto fe_out = drain_collector(*sink);

  require_parity(jit_out, fe_out);
}

SCENARIO("JIT TimeShift drops samples when t + shift < 0", "[trivial][timeshift][skip]") {
  // shift = -10; first 9 ticks (t=1..9) produce negative new_t and must be
  // dropped. Tick 10 onward produces (t-10) >= 0.
  const char* kJson = R"JSON({
    "title": "TimeShift parity negative shift JIT-only since FE throws",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input",     "portTypes": ["number"] },
      { "id": "ts",  "type": "TimeShift", "shift": -10 },
      { "id": "out", "type": "Output",    "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in", "to": "ts",  "fromPort": "o1", "toPort": "i1" },
      { "from": "ts", "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })JSON";

  std::vector<Sample> inputs;
  for (std::int64_t i = 1; i <= 100; ++i) {
    inputs.push_back({i, static_cast<double>(i) * 0.5});
  }

  auto jit_out = run_jit(kJson, inputs);

  // Reference: only ticks with t >= 10 survive; output time is t - 10.
  std::vector<Emit> expected;
  for (const auto& s : inputs) {
    std::int64_t new_t = s.t + (-10);
    if (new_t >= 0) expected.push_back({new_t, s.v});
  }
  require_parity(jit_out, expected);
}

// ---------------------------------------------------------------------------
// TimestampExtract
// ---------------------------------------------------------------------------
SCENARIO("JIT TimestampExtract matches FE interpreter bit-exactly",
         "[trivial][timestampextract]") {
  // The FE TimestampExtract takes VectorNumberData; the JIT treats it the same
  // as any 1->1 stateless transform that ignores its input value. We declare
  // the JSON's input port as "number" so the JIT pipeline accepts our
  // double-valued driver inputs; the operator ignores the value and emits
  // (t, static_cast<double>(t)) regardless.
  const char* kJson = R"({
    "title": "TimestampExtract parity",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input",            "portTypes": ["number"] },
      { "id": "te",  "type": "TimestampExtract" },
      { "id": "out", "type": "Output",           "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in", "to": "te",  "fromPort": "o1", "toPort": "i1" },
      { "from": "te", "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  std::vector<Sample> inputs;
  for (std::int64_t i = 1; i <= 100; ++i) {
    inputs.push_back({i, static_cast<double>(i) * 0.25});
  }

  auto jit_out = run_jit(kJson, inputs);

  // FE reference: drive the actual TimestampExtract with VectorNumberData
  // (the input value is discarded so any vector content works).
  auto op   = rtbot::make_timestamp_extract("te");
  auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  op->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    op->receive_data(
        rtbot::create_message<rtbot::VectorNumberData>(
            s.t, rtbot::VectorNumberData{std::vector<double>{s.v}}),
        0);
  }
  op->execute();
  auto fe_out = drain_collector(*sink);

  require_parity(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// LessThanOrEqualToReplace
// ---------------------------------------------------------------------------
SCENARIO("JIT LessThanOrEqualToReplace matches FE interpreter bit-exactly",
         "[trivial][replace][lte]") {
  const char* kJson = R"({
    "title": "LessThanOrEqualToReplace parity",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input",                    "portTypes": ["number"] },
      { "id": "rp",  "type": "LessThanOrEqualToReplace", "value": 3.0, "replaceBy": 1.0 },
      { "id": "out", "type": "Output",                   "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in", "to": "rp",  "fromPort": "o1", "toPort": "i1" },
      { "from": "rp", "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  // Mix in NaN and +/-Inf to exercise the skip branch.
  auto inputs = make_inputs();

  auto jit_out = run_jit(kJson, inputs);

  auto op   = std::make_shared<rtbot::LessThanOrEqualToReplace>("rp", 3.0, 1.0);
  auto sink = rtbot::make_collector("sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  op->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    op->receive_data(rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}), 0);
  }
  op->execute();
  auto fe_out = drain_collector(*sink);

  require_parity(jit_out, fe_out);
}
