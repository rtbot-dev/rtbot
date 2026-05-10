#ifndef RTBOT_JIT_EMIT_REPLACE_H
#define RTBOT_JIT_EMIT_REPLACE_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // for StatefulOutput

namespace rtbot::jit::emit {

// Stateless 1->1 conditional substitution. Mirrors LessThanOrEqualToReplace:
// drops the message when v is NaN or non-finite (matches the FE's
// `!isnan(v) && isfinite(v)` guard); otherwise emits
// (t, (v <= threshold) ? replace_by : v).
StatefulOutput emit_lte_replace(IrEmissionContext& ec, double threshold,
                                 double replace_by,
                                 llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_REPLACE_H
