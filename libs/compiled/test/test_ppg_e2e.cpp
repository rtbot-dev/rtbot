#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/compiled/ArithmeticStage.h"
#include "rtbot/compiled/CompiledProgram.h"
#include "rtbot/compiled/JoinStage.h"
#include "rtbot/compiled/MovingAverageStage.h"
#include "rtbot/compiled/PeakDetectorStage.h"
#include "rtbot/std/ArithmeticSync.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"

namespace {

constexpr std::size_t kWShort = 5;
constexpr std::size_t kWLong = 30;
constexpr std::size_t kWPeak = 2 * kWShort + 1;  // = 11

// PPG (photoplethysmography) peak-detection pipeline as a composition of
// templated building blocks from libs/compiled. NOT part of the library —
// PPG is a specific realization, the library only contains the per-operator
// stages and the JoinStage primitive. This struct lives in the test
// because the test is what demonstrates "here is how you build PPG from
// the new building blocks and verify it matches the existing operator-graph
// implementation bit-exactly."
//
// Topology mirrors PPGPipelineBenchmark::create_pipeline:
//   input -> ma_short --\
//                        join_ma -> minus -> peak ------\
//   input -> ma_long  --/                                join_out -> emit(peak, original_v)
//   input ------------------------------------------------/
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

std::vector<Pair> drive_pure(const std::vector<double>& values) {
  using namespace rtbot;
  auto input = std::make_shared<Input>("i1", std::vector<std::string>{PortType::NUMBER});
  auto ma_short = std::make_shared<MovingAverage>("ma1", kWShort);
  auto ma_long  = std::make_shared<MovingAverage>("ma2", kWLong);
  auto minus = std::make_shared<Subtraction>("diff");
  auto peak = std::make_shared<PeakDetector>("peak", kWPeak);
  auto join = std::make_shared<Join>("join",
      std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
  auto output = std::make_shared<Output>("o1",
      std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
  auto col = std::make_shared<Collector>("c",
      std::vector<std::string>{PortType::NUMBER, PortType::NUMBER});
  output->connect(col, 0, 0);
  output->connect(col, 1, 1);

  input->connect(ma_short)->connect(minus, 0, 0)->connect(peak)->connect(join, 0, 0)->connect(output);
  input->connect(ma_long)->connect(minus, 0, 1);
  input->connect(join, 0, 1);
  join->connect(output, 1, 1);  // second join port -> output port 1

  std::vector<Pair> out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    input->receive_data(create_message<NumberData>(static_cast<std::int64_t>(i + 1),
                                                    NumberData{values[i]}), 0);
    input->execute();
    auto& q0 = col->get_data_queue(0);
    auto& q1 = col->get_data_queue(1);
    while (out.size() < q0.size() && out.size() < q1.size()) {
      const std::size_t k = out.size();
      const auto* m0 = static_cast<const Message<NumberData>*>(q0[k].get());
      const auto* m1 = static_cast<const Message<NumberData>*>(q1[k].get());
      out.push_back({m0->time, m0->data.value, m1->data.value});
    }
  }
  return out;
}

std::vector<Pair> drive_compiled(const std::vector<double>& values) {
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

}  // namespace

SCENARIO("PPG: compiled pipeline matches operator graph bit-exactly",
         "[ppg][e2e][parity]") {
  const auto values = random_walk(2000);
  const auto pure_out = drive_pure(values);
  const auto comp_out = drive_compiled(values);

  REQUIRE(comp_out.size() == pure_out.size());
  for (std::size_t i = 0; i < pure_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(comp_out[i].t == pure_out[i].t);
    REQUIRE(dbits(comp_out[i].peak_v) == dbits(pure_out[i].peak_v));
    REQUIRE(dbits(comp_out[i].original_v) == dbits(pure_out[i].original_v));
  }
}
