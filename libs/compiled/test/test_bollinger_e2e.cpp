#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Input.h"
#include "rtbot/Output.h"
#include "rtbot/compiled/ArithmeticStage.h"
#include "rtbot/compiled/CompiledProgram.h"
#include "rtbot/compiled/MovingAverageStage.h"
#include "rtbot/compiled/ResamplerHermiteStage.h"
#include "rtbot/compiled/ScaleStage.h"
#include "rtbot/compiled/StdDevStage.h"
#include "rtbot/std/ArithmeticScalar.h"
#include "rtbot/std/ArithmeticSync.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/ResamplerHermite.h"
#include "rtbot/std/StandardDeviation.h"

namespace {

// Bollinger Bands as a composition of templated building blocks from
// libs/compiled. NOT part of the library — Bollinger is a specific
// realization, the library only contains the per-operator stages. This
// struct lives in the test because the test is what demonstrates "here is
// how you build Bollinger from the new building blocks and verify it
// matches the existing operator-graph implementation bit-exactly."
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
      const double lower = sub.process(ma_v, scaled);
      const double upper = add.process(ma_v, scaled);
      emit(rt, lower, upper, ma_v);
    });
  }
};

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

// Builds the same op graph as BollingerBandsPureBenchmark in apps/benchmark.
std::vector<Triple> drive_pure(const std::vector<double>& prices) {
  using namespace rtbot;
  auto input = std::make_shared<Input>("input", std::vector<std::string>{PortType::NUMBER});
  auto resampler = std::make_shared<ResamplerHermite>("resamp", 1);
  auto ma = std::make_shared<MovingAverage>("ma", 14);
  auto sd = std::make_shared<StandardDeviation>("sd", 14);
  auto scale = std::make_shared<Scale>("scale", 2.0);
  auto upper = std::make_shared<Addition>("upper", 2);
  auto lower = std::make_shared<Subtraction>("lower", 2);
  auto output = std::make_shared<Output>(
      "output", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER, PortType::NUMBER});
  auto col = std::make_shared<Collector>(
      "c", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER, PortType::NUMBER});

  output->connect(col, 0, 0);
  output->connect(col, 1, 1);
  output->connect(col, 2, 2);

  input->connect(resampler);
  resampler->connect(ma);
  resampler->connect(sd);
  sd->connect(scale);
  ma->connect(upper);
  ma->connect(lower);
  scale->connect(upper, 0, 1);
  scale->connect(lower, 0, 1);
  upper->connect(output, 0, 1);
  lower->connect(output, 0, 0);
  ma->connect(output, 0, 2);

  std::vector<Triple> out;
  for (std::size_t i = 0; i < prices.size(); ++i) {
    input->receive_data(create_message<NumberData>(static_cast<std::int64_t>(i + 1),
                                                    NumberData{prices[i]}), 0);
    input->execute();
    auto& q0 = col->get_data_queue(0);  // lower
    auto& q1 = col->get_data_queue(1);  // upper
    auto& q2 = col->get_data_queue(2);  // middle
    while (out.size() < q0.size()) {
      const std::size_t i = out.size();
      const auto* m0 = static_cast<const Message<NumberData>*>(q0[i].get());
      const auto* m1 = static_cast<const Message<NumberData>*>(q1[i].get());
      const auto* m2 = static_cast<const Message<NumberData>*>(q2[i].get());
      out.push_back({m0->time, m0->data.value, m1->data.value, m2->data.value});
    }
  }
  return out;
}

std::vector<Triple> drive_compiled(const std::vector<double>& prices) {
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

}  // namespace

SCENARIO("Bollinger Bands: compiled pipeline matches operator graph bit-exactly",
         "[bollinger][e2e][parity]") {
  const auto prices = random_walk(5000);
  const auto pure_out = drive_pure(prices);
  const auto comp_out = drive_compiled(prices);

  REQUIRE(comp_out.size() == pure_out.size());
  for (std::size_t i = 0; i < pure_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(comp_out[i].t == pure_out[i].t);
    REQUIRE(dbits(comp_out[i].lower) == dbits(pure_out[i].lower));
    REQUIRE(dbits(comp_out[i].upper) == dbits(pure_out[i].upper));
    REQUIRE(dbits(comp_out[i].middle) == dbits(pure_out[i].middle));
  }
}
