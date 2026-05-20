#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/compiled/PeakDetectorStage.h"
#include "rtbot/std/PeakDetector.h"

namespace {

using namespace rtbot::compiled;

inline std::uint64_t dbits(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof(double));
  return b;
}

std::vector<double> random_values(std::uint64_t seed, std::size_t n) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(-10.0, 10.0);
  std::vector<double> v(n);
  for (auto& x : v) x = dist(rng);
  return v;
}

struct Output {
  std::int64_t t;
  double v;
};

std::vector<Output> drive_standalone_peak(std::size_t W, const std::vector<double>& values) {
  rtbot::PeakDetector op("peak", W);
  auto col = rtbot::make_number_collector("col");
  op.connect(col, 0, 0);
  std::vector<Output> out;
  std::size_t consumed = 0;
  for (std::size_t i = 0; i < values.size(); ++i) {
    op.receive_data(rtbot::create_message<rtbot::NumberData>(
                         static_cast<std::int64_t>(i + 1),
                         rtbot::NumberData{values[i]}),
                     0);
    op.execute();
    auto& q = col->get_data_queue(0);
    while (consumed < q.size()) {
      const auto* m = static_cast<const rtbot::Message<rtbot::NumberData>*>(q[consumed].get());
      out.push_back({m->time, m->data.value});
      ++consumed;
    }
  }
  return out;
}

template <std::size_t W>
std::vector<Output> drive_stage_peak(const std::vector<double>& values) {
  PeakDetectorStage<W> stage;
  std::vector<Output> out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::int64_t out_t = 0;
    double out_v = 0.0;
    if (stage.process(static_cast<std::int64_t>(i + 1), values[i], out_t, out_v)) {
      out.push_back({out_t, out_v});
    }
  }
  return out;
}

}  // namespace

SCENARIO("PeakDetectorStage matches standalone PeakDetector (W=5)", "[peak][parity]") {
  constexpr std::size_t W = 5;
  const std::size_t N = 500;
  const auto values = random_values(0xBEEF1, N);
  const auto std_out = drive_standalone_peak(W, values);
  const auto stage_out = drive_stage_peak<W>(values);
  REQUIRE(stage_out.size() == std_out.size());
  for (std::size_t i = 0; i < std_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(stage_out[i].t == std_out[i].t);
    REQUIRE(dbits(stage_out[i].v) == dbits(std_out[i].v));
  }
}

SCENARIO("PeakDetectorStage matches standalone PeakDetector (W=11)", "[peak][parity]") {
  constexpr std::size_t W = 11;
  const std::size_t N = 1000;
  const auto values = random_values(0xBEEF2, N);
  const auto std_out = drive_standalone_peak(W, values);
  const auto stage_out = drive_stage_peak<W>(values);
  REQUIRE(stage_out.size() == std_out.size());
  for (std::size_t i = 0; i < std_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(stage_out[i].t == std_out[i].t);
    REQUIRE(dbits(stage_out[i].v) == dbits(std_out[i].v));
  }
}
