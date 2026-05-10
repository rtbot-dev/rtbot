#ifndef RTBOT_COMPILED_PEAK_DETECTOR_STAGE_H
#define RTBOT_COMPILED_PEAK_DETECTOR_STAGE_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace rtbot::compiled {

// Sliding-window peak detector. Window of size W (must be odd, W >= 3) is
// filled, then on every new sample we check whether the center sample
// (index W/2 oldest-first) is strictly greater than all other samples in
// the window. Emits the center value at the center sample's timestamp.
//
// Mirrors rtbot::PeakDetector exactly. Test pins behavior bit-exact.
template <std::size_t W>
struct PeakDetectorStage {
  static_assert(W >= 3, "PeakDetectorStage requires W >= 3");
  static_assert(W % 2 == 1, "PeakDetectorStage requires odd W");

  std::array<double, W> ring_v{};
  std::array<std::int64_t, W> ring_t{};
  std::size_t count{0};

  inline bool process(std::int64_t t, double v,
                      std::int64_t& out_t, double& out_v) noexcept {
    const std::size_t idx = count % W;
    ring_v[idx] = v;
    ring_t[idx] = t;
    ++count;
    if (count < W) return false;

    // Walk the ring oldest-first to find the center. When count == W the
    // ring filled [0..W) in order; otherwise oldest sits at (idx + 1) % W.
    const std::size_t center_step = W / 2;
    const std::size_t center_idx =
        (count == W) ? center_step : (idx + 1 + center_step) % W;
    const double center_v = ring_v[center_idx];

    for (std::size_t k = 0; k < W; ++k) {
      const std::size_t i = (count == W) ? k : (idx + 1 + k) % W;
      if (i != center_idx && ring_v[i] >= center_v) {
        return false;
      }
    }
    out_t = ring_t[center_idx];
    out_v = center_v;
    return true;
  }
};

}  // namespace rtbot::compiled

#endif  // RTBOT_COMPILED_PEAK_DETECTOR_STAGE_H
