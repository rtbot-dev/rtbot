#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/compiled/DiffStage.h"
#include "rtbot/std/Difference.h"

namespace {

using namespace rtbot::compiled;

inline std::uint64_t dbits(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof(double));
  return b;
}

std::vector<double> random_values(std::uint64_t seed, std::size_t n) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<double> v(n);
  for (auto& x : v) x = dist(rng);
  return v;
}

struct DiffOutput {
  std::int64_t t;
  double v;
};

std::vector<DiffOutput> drive_standalone_diff(bool use_oldest, const std::vector<double>& values) {
  rtbot::Difference op("diff", use_oldest);
  auto col = rtbot::make_number_collector("col");
  op.connect(col, 0, 0);
  std::vector<DiffOutput> out;
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

std::vector<DiffOutput> drive_stage_diff(bool use_oldest, const std::vector<double>& values) {
  DiffStage stage{use_oldest};
  std::vector<DiffOutput> out;
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

SCENARIO("DiffStage matches standalone Difference (use_oldest_time=true)", "[diff][parity]") {
  const std::size_t N = 200;
  const auto values = random_values(0xD1FF, N);
  const auto std_out = drive_standalone_diff(true, values);
  const auto stage_out = drive_stage_diff(true, values);
  REQUIRE(stage_out.size() == std_out.size());
  for (std::size_t i = 0; i < std_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(stage_out[i].t == std_out[i].t);
    REQUIRE(dbits(stage_out[i].v) == dbits(std_out[i].v));
  }
}

SCENARIO("DiffStage matches standalone Difference (use_oldest_time=false)", "[diff][parity]") {
  const std::size_t N = 200;
  const auto values = random_values(0xD1F0, N);
  const auto std_out = drive_standalone_diff(false, values);
  const auto stage_out = drive_stage_diff(false, values);
  REQUIRE(stage_out.size() == std_out.size());
  for (std::size_t i = 0; i < std_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(stage_out[i].t == std_out[i].t);
    REQUIRE(dbits(stage_out[i].v) == dbits(std_out[i].v));
  }
}
