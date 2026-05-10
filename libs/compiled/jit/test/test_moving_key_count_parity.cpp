// test_moving_key_count_parity.cpp
//
// Bit-exact parity tests for the JIT MovingKeyCount IR emitter (RB-491).
// Each scenario builds the same MovingKeyCount configuration twice:
//   - JIT side: compile a program JSON via JitCompiler and drive scalar key
//     ticks. The program emits (t, count) for every input.
//   - FE side : construct rtbot::MovingKeyCount directly via the core API,
//     drive the same per-tick keys, and capture emissions through a
//     Collector connected to the operator.
//
// Bit-exact (time, value) equality is required across both runs.

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/std/MovingKeyCount.h"

#include "rtbot/compiled/jit/JitCompiler.h"
#include "rtbot/compiled/jit/JitCompiledProgram.h"

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

struct Sample {
  std::int64_t t;
  double key;
};

struct Emit {
  std::int64_t t;
  double v;
};

std::vector<Emit> run_jit(const std::string& json,
                          const std::vector<Sample>& inputs) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  for (const auto& s : inputs) prog->receive(s.t, s.key);
  std::vector<Emit> out;
  for (const auto& r : prog->collect_outputs()) {
    REQUIRE(r.values.size() == 1);
    out.push_back({r.time, r.values[0]});
  }
  return out;
}

std::vector<Emit> run_fe(std::size_t W, const std::vector<Sample>& inputs) {
  auto mkc = std::make_shared<rtbot::MovingKeyCount>("mkc", W);
  auto sink = std::make_shared<rtbot::Collector>(
      "sink", std::vector<std::string>{"number"});
  mkc->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    mkc->receive_data(rtbot::create_message<rtbot::NumberData>(
                          s.t, rtbot::NumberData{s.key}),
                       0);
  }
  mkc->execute();

  std::vector<Emit> out;
  auto& q = sink->get_data_queue(0);
  for (auto& msg_ptr : q) {
    const auto* msg =
        static_cast<const rtbot::Message<rtbot::NumberData>*>(msg_ptr.get());
    out.push_back({msg->time, msg->data.value});
  }
  q.clear();
  return out;
}

void require_parity(const std::vector<Emit>& jit_out,
                    const std::vector<Emit>& fe_out) {
  REQUIRE(jit_out.size() == fe_out.size());
  for (std::size_t i = 0; i < jit_out.size(); ++i) {
    INFO("emit index " << i << " jit_t=" << jit_out[i].t
                       << " fe_t=" << fe_out[i].t
                       << " jit_v=" << jit_out[i].v
                       << " fe_v=" << fe_out[i].v);
    REQUIRE(jit_out[i].t == fe_out[i].t);
    REQUIRE(dbits(jit_out[i].v) == dbits(fe_out[i].v));
  }
}

std::string make_json(std::size_t W) {
  return std::string(R"({
    "title":"t","apiVersion":"v1","entryOperator":"in",
    "output":{"out":["o1"]},
    "operators":[
      {"id":"in","type":"Input","portTypes":["number"]},
      {"id":"mkc","type":"MovingKeyCount","window_size":)") +
         std::to_string(W) +
         R"(},
      {"id":"out","type":"Output","portTypes":["number"]}
    ],
    "connections":[
      {"from":"in","to":"mkc","fromPort":"o1","toPort":"i1"},
      {"from":"mkc","to":"out","fromPort":"o1","toPort":"i1"}
    ]
  })";
}

}  // namespace

// ---------------------------------------------------------------------------
// SCENARIO 1: Single key drives 20 ticks; counts grow 1..W then stay at W.
// ---------------------------------------------------------------------------
SCENARIO("JIT MovingKeyCount / single key", "[moving_key_count][parity]") {
  constexpr std::size_t W = 5;
  std::vector<Sample> stream;
  for (std::int64_t t = 1; t <= 20; ++t) {
    stream.push_back({t, 42.0});
  }

  auto jit_out = run_jit(make_json(W), stream);
  auto fe_out = run_fe(W, stream);
  require_parity(jit_out, fe_out);
  REQUIRE(jit_out.size() == 20);
}

// ---------------------------------------------------------------------------
// SCENARIO 2: Multiple keys cycling — verifies per-key counts in a window.
// ---------------------------------------------------------------------------
SCENARIO("JIT MovingKeyCount / multiple keys", "[moving_key_count][parity]") {
  constexpr std::size_t W = 7;
  std::vector<Sample> stream;
  // 5 distinct keys cycling across 30 ticks.
  for (std::int64_t t = 1; t <= 30; ++t) {
    const double key = static_cast<double>((t - 1) % 5);
    stream.push_back({t, key});
  }

  auto jit_out = run_jit(make_json(W), stream);
  auto fe_out = run_fe(W, stream);
  require_parity(jit_out, fe_out);
  REQUIRE(jit_out.size() == 30);
}

// ---------------------------------------------------------------------------
// SCENARIO 3: Window roll-off — keys cycle through and counts decrement
// correctly when keys leave the window.
// ---------------------------------------------------------------------------
SCENARIO("JIT MovingKeyCount / window roll-off",
         "[moving_key_count][parity]") {
  constexpr std::size_t W = 4;
  std::vector<Sample> stream;
  // Pattern designed to exercise eviction edges: bursts of the same key
  // followed by switches, across 50 ticks.
  for (std::int64_t t = 1; t <= 50; ++t) {
    double key;
    const auto block = (t - 1) / 3;  // change key every 3 ticks
    switch (block % 4) {
      case 0: key = 1.0; break;
      case 1: key = 2.0; break;
      case 2: key = 3.0; break;
      default: key = 1.0; break;  // re-introduce key 1 to test mid-stream re-counting
    }
    stream.push_back({t, key});
  }

  auto jit_out = run_jit(make_json(W), stream);
  auto fe_out = run_fe(W, stream);
  require_parity(jit_out, fe_out);
  REQUIRE(jit_out.size() == 50);
}
