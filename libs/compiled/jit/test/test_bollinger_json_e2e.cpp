#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Program.h"
#include "rtbot/compiled/ArithmeticStage.h"
#include "rtbot/compiled/CompiledProgram.h"
#include "rtbot/compiled/MovingAverageStage.h"
#include "rtbot/compiled/ResamplerHermiteStage.h"
#include "rtbot/compiled/ScaleStage.h"
#include "rtbot/compiled/StdDevStage.h"
#include "rtbot/compiled/jit/JitCompiler.h"

namespace {

// Same Bollinger JSON as test_json_parser.cpp (and apps/benchmark/src/benchmark.cpp).
const char* kBollingerJson = R"({
  "title": "Bollinger Bands",
  "apiVersion": "v1",
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

inline std::uint64_t dbits(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof(double));
  return b;
}

std::vector<double> random_walk(std::size_t n, std::uint64_t seed = 0xB011) {
  std::mt19937_64 rng(seed);
  std::normal_distribution<double> step(0.0, 1.0);
  std::vector<double> v(n);
  double price = 100.0;
  for (auto& x : v) { price += step(rng); x = price; }
  return v;
}

struct Triple { std::int64_t t; double lower, upper, middle; };

// -----------------------------------------------------------------------
// Path 1: FE interpreter via rtbot::Program constructed from JSON.
//
// The JSON output mapping is: "37" -> ["o2", "o1", "o3"]
//   o2 = Output port 1 = upper (Addition "861" -> "37" toPort "i2")
//   o1 = Output port 0 = lower (Subtraction "495" -> "37" toPort "i1")
//   o3 = Output port 2 = middle (MovingAverage "510" -> "37" toPort "i3")
//
// Program::receive() returns a ProgramMsgBatch keyed by upstream op id
// and port name "oN". The sink wires:
//   sink port 0 <- "37" output port 1 (upper)  -> batch["37"]["o2"]
//   sink port 1 <- "37" output port 0 (lower)  -> batch["37"]["o1"]
//   sink port 2 <- "37" output port 2 (middle) -> batch["37"]["o3"]
// -----------------------------------------------------------------------
std::vector<Triple> drive_fe(const std::vector<double>& prices) {
  using namespace rtbot;

  Program prog(kBollingerJson);
  std::vector<Triple> out;

  for (std::size_t i = 0; i < prices.size(); ++i) {
    auto batch = prog.receive(
        Message<NumberData>(static_cast<std::int64_t>(i + 1), NumberData{prices[i]}));

    auto op_it = batch.find("37");
    if (op_it == batch.end()) continue;

    auto& op_batch = op_it->second;
    auto lower_it  = op_batch.find("o1");
    auto upper_it  = op_batch.find("o2");
    auto middle_it = op_batch.find("o3");
    if (lower_it == op_batch.end() || upper_it == op_batch.end() ||
        middle_it == op_batch.end()) continue;

    const auto& lower_msgs  = lower_it->second;
    const auto& upper_msgs  = upper_it->second;
    const auto& middle_msgs = middle_it->second;

    const std::size_t n = lower_msgs.size();
    for (std::size_t k = 0; k < n; ++k) {
      const auto* ml = static_cast<const Message<NumberData>*>(lower_msgs[k].get());
      const auto* mu = static_cast<const Message<NumberData>*>(upper_msgs[k].get());
      const auto* mm = static_cast<const Message<NumberData>*>(middle_msgs[k].get());
      out.push_back({ml->time, ml->data.value, mu->data.value, mm->data.value});
    }
  }

  return out;
}

// -----------------------------------------------------------------------
// Path 2: AOT template pipeline via CompiledProgram.
//
// BollingerCompiled<1,14,2>::process emits (rt, lower, upper, ma_v).
// CompiledProgram captures into r.values[0]=lower, [1]=upper, [2]=middle.
// -----------------------------------------------------------------------
template <std::int64_t Interval, std::size_t W, int K_int>
struct BollingerCompiled {
  rtbot::compiled::ResamplerHermiteStage resampler{Interval};
  rtbot::compiled::MovingAverageStage<W> ma;
  rtbot::compiled::StdDevStage<W> sd;
  rtbot::compiled::ScaleStage scale{static_cast<double>(K_int)};
  rtbot::compiled::AdditionStage add;
  rtbot::compiled::SubtractionStage sub;

  template <class F>
  inline void process(std::int64_t t, double v, F&& emit) noexcept {
    resampler.process(t, v, [&](std::int64_t rt, double rv) {
      std::int64_t ma_t = 0, sd_t = 0;
      double ma_v = 0.0, sd_v = 0.0;
      const bool ma_ok = ma.process(rt, rv, ma_t, ma_v);
      const bool sd_ok = sd.process(rt, rv, sd_t, sd_v);
      if (!(ma_ok && sd_ok)) return;
      const double scaled = scale.process(sd_v);
      const double lower  = sub.process(ma_v, scaled);
      const double upper  = add.process(ma_v, scaled);
      emit(rt, lower, upper, ma_v);
    });
  }
};

std::vector<Triple> drive_aot(const std::vector<double>& prices) {
  rtbot::compiled::CompiledProgram<BollingerCompiled<1, 14, 2>, 3> prog;
  for (std::size_t i = 0; i < prices.size(); ++i) {
    prog.receive(static_cast<std::int64_t>(i + 1), prices[i]);
  }
  std::vector<Triple> out;
  for (const auto& r : prog.collect_outputs()) {
    out.push_back({r.time, r.values[0], r.values[1], r.values[2]});
  }
  return out;
}

// -----------------------------------------------------------------------
// Path 3: JIT-compiled via JitCompiler::compile(bollinger_json).
//
// The JIT output array is indexed by to_port of each connection into the
// Output op:
//   out_v[0] = lower (495->37 toPort "i1" -> port 0)
//   out_v[1] = upper (861->37 toPort "i2" -> port 1)
//   out_v[2] = middle (510->37 toPort "i3" -> port 2)
// -----------------------------------------------------------------------
std::vector<Triple> drive_jit(const std::vector<double>& prices) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(kBollingerJson);

  for (std::size_t i = 0; i < prices.size(); ++i) {
    prog->receive(static_cast<std::int64_t>(i + 1), prices[i]);
  }

  std::vector<Triple> out;
  for (const auto& r : prog->collect_outputs()) {
    // r.values[0]=lower, r.values[1]=upper, r.values[2]=middle
    out.push_back({r.time, r.values[0], r.values[1], r.values[2]});
  }
  return out;
}

}  // namespace

SCENARIO("JIT-compiled Bollinger from JSON matches FE interpreter and AOT template",
         "[bollinger][jit][e2e][parity]") {
  const auto prices = random_walk(5000);

  const auto fe_out  = drive_fe(prices);
  const auto aot_out = drive_aot(prices);
  const auto jit_out = drive_jit(prices);

  // All three paths must produce the same number of outputs.
  REQUIRE(fe_out.size() == aot_out.size());
  REQUIRE(fe_out.size() == jit_out.size());
  // Must produce some outputs (warmup is 14 ticks, so ~4987 should emit).
  REQUIRE(fe_out.size() > 4900u);

  for (std::size_t i = 0; i < fe_out.size(); ++i) {
    INFO("i=" << i
              << " fe_t=" << fe_out[i].t
              << " aot_t=" << aot_out[i].t
              << " jit_t=" << jit_out[i].t);

    // Timestamps
    REQUIRE(fe_out[i].t == aot_out[i].t);
    REQUIRE(fe_out[i].t == jit_out[i].t);

    // lower
    REQUIRE(dbits(fe_out[i].lower) == dbits(aot_out[i].lower));
    REQUIRE(dbits(fe_out[i].lower) == dbits(jit_out[i].lower));

    // upper
    REQUIRE(dbits(fe_out[i].upper) == dbits(aot_out[i].upper));
    REQUIRE(dbits(fe_out[i].upper) == dbits(jit_out[i].upper));

    // middle
    REQUIRE(dbits(fe_out[i].middle) == dbits(aot_out[i].middle));
    REQUIRE(dbits(fe_out[i].middle) == dbits(jit_out[i].middle));
  }
}
