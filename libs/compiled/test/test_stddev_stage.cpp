#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "rtbot/compiled/StdDevStage.h"
#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedScalarEval.h"
#include "rtbot/fuse/FusedStateLayout.h"

namespace {

using namespace rtbot::compiled;
using namespace rtbot::fuse;
using namespace rtbot::fused_op;

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

std::vector<double> drive_fe_std(std::size_t W, const std::vector<double>& values) {
  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(STD_UPDATE), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  std::vector<AuxArgs> aux = {{0, static_cast<std::uint16_t>(W), 0, 0}};
  auto layout = compute_state_layout(packed, aux);
  std::vector<double> state = layout.initial_values;
  std::vector<double> out;
  for (double v : values) {
    double scratch = 0.0;
    const double inputs[1] = {v};
    if (evaluate_one(packed.data(), packed.size(), nullptr, aux.data(),
                     nullptr, inputs, state.data(), &scratch, 1)) {
      out.push_back(scratch);
    }
  }
  return out;
}

template <std::size_t W>
std::vector<double> drive_stage_std(const std::vector<double>& values) {
  StdDevStage<W> stage;
  std::vector<double> out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    int64_t out_t = 0;
    double out_v = 0.0;
    if (stage.process(static_cast<int64_t>(i + 1), values[i], out_t, out_v)) {
      out.push_back(out_v);
    }
  }
  return out;
}

}  // namespace

SCENARIO("StdDevStage matches FE STD_UPDATE bit-exactly", "[std][parity]") {
  constexpr std::size_t W = 14;
  const std::size_t N = 500;
  const auto values = random_values(0x57D1, N);

  const auto fe_out = drive_fe_std(W, values);
  const auto stage_out = drive_stage_std<W>(values);

  REQUIRE(fe_out.size() == N - W + 1);
  REQUIRE(stage_out.size() == fe_out.size());
  for (std::size_t i = 0; i < fe_out.size(); ++i) {
    INFO("i=" << i);
    REQUIRE(dbits(stage_out[i]) == dbits(fe_out[i]));
  }
}
