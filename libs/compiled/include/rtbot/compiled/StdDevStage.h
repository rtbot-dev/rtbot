#ifndef RTBOT_COMPILED_STDDEV_STAGE_H
#define RTBOT_COMPILED_STDDEV_STAGE_H

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace rtbot::compiled {

template <std::size_t W>
struct StdDevStage {
  static_assert(W >= 2, "StdDevStage requires W >= 2 for sample variance");

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

    const double mean = sum / static_cast<double>(W);
    double m2 = 0.0;
    for (std::size_t k = 0; k < W; ++k) {
      std::size_t ring_idx;
      if (count == W) {
        ring_idx = k;
      } else {
        ring_idx = (idx + 1 + k) % W;
      }
      const double d = ring[ring_idx] - mean;
      m2 += d * d;
    }
    out_t = t;
    out_v = std::sqrt(m2 / static_cast<double>(W - 1));
    return true;
  }
};

}  // namespace rtbot::compiled

#endif  // RTBOT_COMPILED_STDDEV_STAGE_H
