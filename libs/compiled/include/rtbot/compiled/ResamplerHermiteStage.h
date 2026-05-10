#ifndef RTBOT_COMPILED_RESAMPLER_HERMITE_STAGE_H
#define RTBOT_COMPILED_RESAMPLER_HERMITE_STAGE_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace rtbot::compiled {

// Hermite cubic interpolation resampler. Buffers the last 4 samples and
// emits interpolated values at fixed intervals dt starting from buffer[1]'s
// time. May emit 0, 1, or many values per input depending on the spacing
// between inputs and dt.
//
// Mirrors rtbot::ResamplerHermite (libs/std/include/rtbot/std/ResamplerHermite.h)
// exactly. The hermite_interpolate helper is copied verbatim.
struct ResamplerHermiteStage {
  std::int64_t dt;
  std::array<double, 4> ring_v{};
  std::array<std::int64_t, 4> ring_t{};
  std::size_t count{0};
  std::int64_t next_emit{0};
  bool initialized{false};

  explicit ResamplerHermiteStage(std::int64_t interval) noexcept : dt(interval) {}

  template <class F>
  inline void process(std::int64_t t, double v, F&& emit) noexcept {
    // Push to the 4-sample ring (FIFO).
    if (count < 4) {
      ring_v[count] = v;
      ring_t[count] = t;
      ++count;
    } else {
      // Shift left.
      ring_v[0] = ring_v[1]; ring_v[1] = ring_v[2]; ring_v[2] = ring_v[3]; ring_v[3] = v;
      ring_t[0] = ring_t[1]; ring_t[1] = ring_t[2]; ring_t[2] = ring_t[3]; ring_t[3] = t;
    }

    if (!initialized) {
      if (count < 4) return;
      next_emit = ring_t[1];
      initialized = true;
    }

    // Emit while next_emit lies in [ring_t[1], ring_t[2]].
    while (ring_t[1] <= next_emit && next_emit <= ring_t[2]) {
      const double mu = static_cast<double>(next_emit - ring_t[1])
                      / static_cast<double>(ring_t[2] - ring_t[1]);
      const double y0 = ring_v[0], y1 = ring_v[1], y2 = ring_v[2], y3 = ring_v[3];
      // Hermite cubic — verbatim copy of rtbot::ResamplerHermite::hermite_interpolate
      // with tension=0, bias=0. The (1+bias)*(1-tension)/2 form is preserved
      // intentionally to guarantee bit-exact parity under -fno-associative-math.
      const double m0 = ((y1 - y0) * (1 + 0.0) * (1 - 0.0) / 2) + ((y2 - y1) * (1 - 0.0) * (1 - 0.0) / 2);
      const double m1 = ((y2 - y1) * (1 + 0.0) * (1 - 0.0) / 2) + ((y3 - y2) * (1 - 0.0) * (1 - 0.0) / 2);
      const double mu2 = mu * mu;
      const double mu3 = mu2 * mu;
      const double h00 = 2.0 * mu3 - 3.0 * mu2 + 1.0;
      const double h10 = mu3 - 2.0 * mu2 + mu;
      const double h01 = -2.0 * mu3 + 3.0 * mu2;
      const double h11 = mu3 - mu2;
      const double interpolated = h00 * y1 + h10 * m0 + h01 * y2 + h11 * m1;
      emit(next_emit, interpolated);
      next_emit += dt;
    }
  }
};

}  // namespace rtbot::compiled

#endif  // RTBOT_COMPILED_RESAMPLER_HERMITE_STAGE_H
