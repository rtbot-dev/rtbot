#ifndef RTBOT_COMPILED_MOVING_AVERAGE_STAGE_H
#define RTBOT_COMPILED_MOVING_AVERAGE_STAGE_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace rtbot::compiled {

template <std::size_t W>
struct MovingAverageStage {
  static_assert(W >= 1, "MovingAverageStage requires W >= 1");

  std::array<double, W> ring{};
  double sum{0.0};
  double comp{0.0};
  std::size_t count{0};

  inline bool process(std::int64_t t, double v,
                      std::int64_t& out_t, double& out_v) noexcept {
    const std::size_t idx = count % W;
    if (count >= W) {
      const double leaving = ring[idx];
      const double ys = (-leaving) - comp;
      const double ts = sum + ys;
      comp = (ts - sum) - ys;
      sum = ts;
    }
    ring[idx] = v;
    const double ya = v - comp;
    const double ta = sum + ya;
    comp = (ta - sum) - ya;
    sum = ta;
    ++count;
    if (count < W) return false;
    out_t = t;
    out_v = sum / static_cast<double>(W);
    return true;
  }
};

}  // namespace rtbot::compiled

#endif  // RTBOT_COMPILED_MOVING_AVERAGE_STAGE_H
