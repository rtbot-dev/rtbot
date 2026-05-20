#ifndef RTBOT_COMPILED_ARITHMETIC_STAGE_H
#define RTBOT_COMPILED_ARITHMETIC_STAGE_H

#include <type_traits>

namespace rtbot::compiled {

struct AddOp {};
struct SubOp {};
struct MulOp {};
struct DivOp {};

template <class Op>
struct ArithmeticStage {
  inline double process(double a, double b) const noexcept {
    if constexpr (std::is_same_v<Op, AddOp>) {
      return a + b;
    } else if constexpr (std::is_same_v<Op, SubOp>) {
      return a - b;
    } else if constexpr (std::is_same_v<Op, MulOp>) {
      return a * b;
    } else {
      static_assert(std::is_same_v<Op, DivOp>,
                    "ArithmeticStage: unsupported Op tag");
      return a / b;
    }
  }
};

using AdditionStage = ArithmeticStage<AddOp>;
using SubtractionStage = ArithmeticStage<SubOp>;
using MultiplicationStage = ArithmeticStage<MulOp>;
using DivisionStage = ArithmeticStage<DivOp>;

}  // namespace rtbot::compiled

#endif  // RTBOT_COMPILED_ARITHMETIC_STAGE_H
