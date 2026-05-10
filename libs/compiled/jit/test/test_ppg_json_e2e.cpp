#include <catch2/catch.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Program.h"
#include "rtbot/compiled/ArithmeticStage.h"
#include "rtbot/compiled/CompiledProgram.h"
#include "rtbot/compiled/JoinStage.h"
#include "rtbot/compiled/MovingAverageStage.h"
#include "rtbot/compiled/PeakDetectorStage.h"
#include "rtbot/compiled/jit/JitCompiler.h"

namespace {

// PPG peak-detection pipeline (mirrors AOT PPGCompiled exactly):
//   input -> ma_short --\
//                        join_ma -> minus -> peak -----\
//   input -> ma_long  --/                               join_out -> emit
//   input -----------------------------------------------/
//
// WShort=5, WLong=30, WPeak=11
//
// Two explicit Join operators are required:
//   join_ma: synchronises ma_short (port 0) and ma_long (port 1) before minus
//   join_out: synchronises peak (port 0) and raw input (port 1) for output
//
// Output field maps "out" -> ["o1", "o2"]:
//   o1 = join_out port 0 = peak_v     (peak -> join_out i1 -> out i1)
//   o2 = join_out port 1 = original_v (input -> join_out i2 -> out i2)
const char* kPpgJson = R"({
  "title": "PPG Peak Detection",
  "apiVersion": "v1",
  "entryOperator": "input",
  "output": { "out": ["o1", "o2"] },
  "operators": [
    { "id": "input",    "type": "Input",         "portTypes": ["number"] },
    { "id": "ma_short", "type": "MovingAverage", "window_size": 5 },
    { "id": "ma_long",  "type": "MovingAverage", "window_size": 30 },
    { "id": "join_ma",  "type": "Join",          "portTypes": ["number", "number"] },
    { "id": "minus",    "type": "Subtraction" },
    { "id": "peak",     "type": "PeakDetector",  "window_size": 11 },
    { "id": "join_out", "type": "Join",          "portTypes": ["number", "number"] },
    { "id": "out",      "type": "Output",        "portTypes": ["number", "number"] }
  ],
  "connections": [
    { "from": "input",    "to": "ma_short",  "fromPort": "o1", "toPort": "i1" },
    { "from": "input",    "to": "ma_long",   "fromPort": "o1", "toPort": "i1" },
    { "from": "ma_short", "to": "join_ma",   "fromPort": "o1", "toPort": "i1" },
    { "from": "ma_long",  "to": "join_ma",   "fromPort": "o1", "toPort": "i2" },
    { "from": "join_ma",  "to": "minus",     "fromPort": "o1", "toPort": "i1" },
    { "from": "join_ma",  "to": "minus",     "fromPort": "o2", "toPort": "i2" },
    { "from": "minus",    "to": "peak",      "fromPort": "o1", "toPort": "i1" },
    { "from": "peak",     "to": "join_out",  "fromPort": "o1", "toPort": "i1" },
    { "from": "input",    "to": "join_out",  "fromPort": "o1", "toPort": "i2" },
    { "from": "join_out", "to": "out",       "fromPort": "o1", "toPort": "i1" },
    { "from": "join_out", "to": "out",       "fromPort": "o2", "toPort": "i2" }
  ]
})";

inline std::uint64_t dbits(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof(double));
  return b;
}

std::vector<double> random_walk(std::size_t n, std::uint64_t seed = 0xBE6F) {
  std::mt19937_64 rng(seed);
  std::normal_distribution<double> step(0.0, 1.0);
  std::vector<double> v(n);
  double x = 100.0;
  for (auto& y : v) { x += step(rng); y = x; }
  return v;
}

struct Pair { std::int64_t t; double peak_v, original_v; };

// ---------------------------------------------------------------------------
// Path 1: FE interpreter via rtbot::Program constructed from JSON.
//
// The JSON output mapping is: "out" -> ["o1", "o2"]
//   o1 = Output port 0 = peak_v    (join o1 -> out i1)
//   o2 = Output port 1 = original_v (join o2 -> out i2)
//
// Program::receive() returns a ProgramMsgBatch keyed by upstream op id
// and port name "oN". The sink wires follow the output mapping order:
//   batch["out"]["o1"] = peak_v messages
//   batch["out"]["o2"] = original_v messages
// ---------------------------------------------------------------------------
std::vector<Pair> drive_fe(const std::vector<double>& values) {
  using namespace rtbot;

  Program prog(kPpgJson);
  std::vector<Pair> out;

  for (std::size_t i = 0; i < values.size(); ++i) {
    auto batch = prog.receive(
        Message<NumberData>(static_cast<std::int64_t>(i + 1), NumberData{values[i]}));

    auto op_it = batch.find("out");
    if (op_it == batch.end()) continue;

    auto& op_batch = op_it->second;
    auto peak_it     = op_batch.find("o1");
    auto original_it = op_batch.find("o2");
    if (peak_it == op_batch.end() || original_it == op_batch.end()) continue;

    const auto& peak_msgs     = peak_it->second;
    const auto& original_msgs = original_it->second;

    const std::size_t n = peak_msgs.size();
    for (std::size_t k = 0; k < n; ++k) {
      const auto* mp = static_cast<const Message<NumberData>*>(peak_msgs[k].get());
      const auto* mo = static_cast<const Message<NumberData>*>(original_msgs[k].get());
      out.push_back({mp->time, mp->data.value, mo->data.value});
    }
  }

  return out;
}

// ---------------------------------------------------------------------------
// Path 2: AOT template pipeline via CompiledProgram.
//
// PPGCompiled<5,30,11>::process emits (t, peak_v, original_v).
// CompiledProgram captures: r.values[0]=peak_v, r.values[1]=original_v.
// ---------------------------------------------------------------------------
template <std::size_t WShort, std::size_t WLong, std::size_t WPeak>
struct PPGCompiled {
  static_assert(WPeak >= 3 && (WPeak % 2 == 1), "WPeak must be odd >= 3");

  rtbot::compiled::MovingAverageStage<WShort> ma_short;
  rtbot::compiled::MovingAverageStage<WLong> ma_long;
  rtbot::compiled::JoinStage<2> join_ma;
  rtbot::compiled::SubtractionStage minus;
  rtbot::compiled::PeakDetectorStage<WPeak> peak;
  rtbot::compiled::JoinStage<2> join_out;

  template <class F>
  inline void process(std::int64_t t, double v, F&& emit) noexcept {
    push_join_out_port_1(t, v, emit);

    std::int64_t ms_t = 0; double ms_v = 0.0;
    if (ma_short.process(t, v, ms_t, ms_v)) {
      push_join_ma(0, ms_t, ms_v, emit);
    }
    std::int64_t ml_t = 0; double ml_v = 0.0;
    if (ma_long.process(t, v, ml_t, ml_v)) {
      push_join_ma(1, ml_t, ml_v, emit);
    }
  }

 private:
  template <class F>
  inline void push_join_out_port_1(std::int64_t t, double v, F& emit) noexcept {
    std::int64_t out_t = 0;
    std::array<double, 2> out_vs{};
    if (join_out.push(1, t, v, out_t, out_vs)) {
      emit(out_t, out_vs[0], out_vs[1]);
    }
  }

  template <class F>
  inline void push_join_out_port_0(std::int64_t t, double v, F& emit) noexcept {
    std::int64_t out_t = 0;
    std::array<double, 2> out_vs{};
    if (join_out.push(0, t, v, out_t, out_vs)) {
      emit(out_t, out_vs[0], out_vs[1]);
    }
  }

  template <class F>
  inline void push_join_ma(std::size_t port, std::int64_t t, double v, F& emit) noexcept {
    std::int64_t join_t = 0;
    std::array<double, 2> join_vs{};
    if (join_ma.push(port, t, v, join_t, join_vs)) {
      const double diff = minus.process(join_vs[0], join_vs[1]);
      std::int64_t pk_t = 0;
      double pk_v = 0.0;
      if (peak.process(join_t, diff, pk_t, pk_v)) {
        push_join_out_port_0(pk_t, pk_v, emit);
      }
    }
  }
};

std::vector<Pair> drive_aot(const std::vector<double>& values) {
  constexpr std::size_t kWShort = 5;
  constexpr std::size_t kWLong  = 30;
  constexpr std::size_t kWPeak  = 11;
  rtbot::compiled::CompiledProgram<PPGCompiled<kWShort, kWLong, kWPeak>, 2> prog;
  for (std::size_t i = 0; i < values.size(); ++i) {
    prog.receive(static_cast<std::int64_t>(i + 1), values[i]);
  }
  std::vector<Pair> out;
  for (const auto& r : prog.collect_outputs()) {
    out.push_back({r.time, r.values[0], r.values[1]});
  }
  return out;
}

// ---------------------------------------------------------------------------
// Path 3 (NEW): JIT-compiled via JitCompiler::compile(kPpgJson).
//
// The JIT output array is indexed by to_port of each connection into the
// Output op:
//   out_v[0] = peak_v     (join->out toPort "i1" -> port 0)
//   out_v[1] = original_v (join->out toPort "i2" -> port 1)
// ---------------------------------------------------------------------------
std::vector<Pair> drive_jit(const std::vector<double>& values) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(kPpgJson);

  for (std::size_t i = 0; i < values.size(); ++i) {
    prog->receive(static_cast<std::int64_t>(i + 1), values[i]);
  }

  std::vector<Pair> out;
  for (const auto& r : prog->collect_outputs()) {
    // r.values[0]=peak_v, r.values[1]=original_v
    out.push_back({r.time, r.values[0], r.values[1]});
  }
  return out;
}

}  // namespace

SCENARIO("PPG via JSON: JIT matches FE interpreter and AOT template bit-exactly",
         "[ppg][jit][e2e][parity]") {
  const auto values = random_walk(2000);

  const auto fe_out  = drive_fe(values);
  const auto aot_out = drive_aot(values);
  const auto jit_out = drive_jit(values);

  // All three paths must produce the same number of outputs.
  REQUIRE(fe_out.size() == aot_out.size());
  REQUIRE(fe_out.size() == jit_out.size());
  // PeakDetector emits only at local maxima: ~97 peaks expected from 2000 random-walk samples.
  REQUIRE(fe_out.size() > 0u);

  for (std::size_t i = 0; i < fe_out.size(); ++i) {
    INFO("i=" << i
              << " fe_t=" << fe_out[i].t
              << " aot_t=" << aot_out[i].t
              << " jit_t=" << jit_out[i].t);

    // Timestamps
    REQUIRE(fe_out[i].t == aot_out[i].t);
    REQUIRE(fe_out[i].t == jit_out[i].t);

    // peak_v
    REQUIRE(dbits(fe_out[i].peak_v) == dbits(aot_out[i].peak_v));
    REQUIRE(dbits(fe_out[i].peak_v) == dbits(jit_out[i].peak_v));

    // original_v
    REQUIRE(dbits(fe_out[i].original_v) == dbits(aot_out[i].original_v));
    REQUIRE(dbits(fe_out[i].original_v) == dbits(jit_out[i].original_v));
  }
}
