#ifndef RTBOT_COMPILED_SCALE_STAGE_H
#define RTBOT_COMPILED_SCALE_STAGE_H

namespace rtbot::compiled {

struct ScaleStage {
  double k;

  explicit ScaleStage(double k_) noexcept : k(k_) {}

  inline double process(double v) const noexcept { return k * v; }
};

}  // namespace rtbot::compiled

#endif  // RTBOT_COMPILED_SCALE_STAGE_H
