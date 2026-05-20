#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/compiled/ResamplerHermiteStage.h"
#include "rtbot/std/ResamplerHermite.h"

namespace {

using namespace rtbot::compiled;

inline std::uint64_t dbits(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof(double));
  return b;
}

struct Output {
  std::int64_t t;
  double v;
};

std::vector<Output> drive_standalone_resampler(
    std::int64_t interval,
    const std::vector<std::pair<std::int64_t, double>>& inputs) {
  rtbot::ResamplerHermite op("r", interval);
  auto col = rtbot::make_number_collector("col");
  op.connect(col, 0, 0);
  std::vector<Output> out;
  std::size_t consumed = 0;
  for (auto [t, v] : inputs) {
    op.receive_data(rtbot::create_message<rtbot::NumberData>(t, rtbot::NumberData{v}), 0);
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

std::vector<Output> drive_stage_resampler(
    std::int64_t interval,
    const std::vector<std::pair<std::int64_t, double>>& inputs) {
  ResamplerHermiteStage stage{interval};
  std::vector<Output> out;
  for (auto [t, v] : inputs) {
    stage.process(t, v, [&](std::int64_t et, double ev) {
      out.push_back({et, ev});
    });
  }
  return out;
}

}  // namespace

SCENARIO("ResamplerHermiteStage matches standalone for unit-spaced input",
         "[resampler][parity]") {
  std::vector<std::pair<std::int64_t, double>> inputs;
  std::mt19937_64 rng(0xCAFE);
  std::uniform_real_distribution<double> dist(-10.0, 10.0);
  for (std::int64_t t = 1; t <= 200; ++t) {
    inputs.emplace_back(t, dist(rng));
  }
  const auto std_out = drive_standalone_resampler(1, inputs);
  const auto stage_out = drive_stage_resampler(1, inputs);
  REQUIRE(stage_out.size() == std_out.size());
  for (std::size_t i = 0; i < std_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(stage_out[i].t == std_out[i].t);
    REQUIRE(dbits(stage_out[i].v) == dbits(std_out[i].v));
  }
}

SCENARIO("ResamplerHermiteStage matches standalone for sparse input (multi-emit)",
         "[resampler][parity]") {
  // Sparse input: timestamps spaced by 5, interval=1 means each input
  // produces multiple interpolated emissions.
  std::vector<std::pair<std::int64_t, double>> inputs;
  std::mt19937_64 rng(0xCA5E);
  std::uniform_real_distribution<double> dist(-10.0, 10.0);
  for (std::int64_t k = 0; k < 100; ++k) {
    inputs.emplace_back(1 + k * 5, dist(rng));
  }
  const auto std_out = drive_standalone_resampler(1, inputs);
  const auto stage_out = drive_stage_resampler(1, inputs);
  REQUIRE(stage_out.size() == std_out.size());
  for (std::size_t i = 0; i < std_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(stage_out[i].t == std_out[i].t);
    REQUIRE(dbits(stage_out[i].v) == dbits(std_out[i].v));
  }
}
