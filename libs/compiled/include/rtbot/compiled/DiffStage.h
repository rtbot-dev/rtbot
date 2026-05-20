#ifndef RTBOT_COMPILED_DIFF_STAGE_H
#define RTBOT_COMPILED_DIFF_STAGE_H

#include <cstddef>
#include <cstdint>

namespace rtbot::compiled {

// Two-sample first-order difference. Matches rtbot::Difference: emits
// buffer[1].value - buffer[0].value once two samples are seen, at either
// the newer (use_oldest_time=true, the default) or older sample's time.
struct DiffStage {
  bool use_oldest_time;
  double prev_v{0.0};
  std::int64_t prev_t{0};
  std::int64_t curr_t{0};
  std::size_t count{0};

  explicit DiffStage(bool use_oldest = true) noexcept
      : use_oldest_time(use_oldest) {}

  inline bool process(std::int64_t t, double v,
                      std::int64_t& out_t, double& out_v) noexcept {
    if (count == 0) {
      prev_v = v;
      prev_t = t;
      ++count;
      return false;
    }
    out_v = v - prev_v;
    out_t = use_oldest_time ? t : prev_t;
    // Advance the 2-sample window.
    prev_v = v;
    prev_t = t;
    ++count;
    return true;
  }
};

}  // namespace rtbot::compiled

#endif  // RTBOT_COMPILED_DIFF_STAGE_H
