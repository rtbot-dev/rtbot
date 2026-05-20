#ifndef RTBOT_JIT_EMIT_FUNCTION_H
#define RTBOT_JIT_EMIT_FUNCTION_H

#include <cstddef>
#include <utility>
#include <vector>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Stateless 1->1 piecewise interpolator. The point table and (for Hermite)
// pre-computed tangents are baked into the IR as private global constant
// arrays. Returns the interpolated value at x; the caller preserves the
// input timestamp.
//
// `points` must be sorted ascending by x and have size >= 2.
// `use_hermite` selects between LINEAR and HERMITE interpolation; tangents
// are only consulted when use_hermite is true and must have size points.size().
//
// The IR mirrors Function::interpolate's FP arithmetic order verbatim for
// bit-exact parity with the FE interpreter.
llvm::Value* emit_function(IrEmissionContext& ec,
                           const std::vector<std::pair<double, double>>& points,
                           bool use_hermite,
                           const std::vector<double>& tangents,
                           llvm::Value* x);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_FUNCTION_H
