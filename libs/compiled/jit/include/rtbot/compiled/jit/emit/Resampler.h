#ifndef RTBOT_JIT_EMIT_RESAMPLER_H
#define RTBOT_JIT_EMIT_RESAMPLER_H

#include <cstddef>
#include <cstdint>
#include <functional>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Callback invoked for each interpolated emission inside the resampler loop.
// The IRBuilder is positioned inside the loop body when the callback fires.
// (out_t: i64, out_v: double)
using ResamplerEmitCallback = std::function<void(llvm::Value* /*out_t*/,
                                                  llvm::Value* /*out_v*/)>;

// Emit IR for one ResamplerHermite step at the given state offset.
//
// State layout (11 doubles at state_offset):
//   [0..3]  ring_v[4]       — value ring
//   [4..7]  ring_t[4]       — timestamp ring (bit-cast as double)
//   [8]     count           — sample count (double)
//   [9]     initialized     — 0.0=false / 1.0=true
//   [10]    next_emit       — next emission timestamp (bit-cast as double)
//
// interval is the fixed output sample interval (dt), baked in as an i64
// constant.
//
// The callback is invoked (possibly multiple times) with (out_t, out_v) for
// each interpolated emission. The IRBuilder is positioned in the loop body
// on each invocation. After each callback, next_emit is advanced by interval.
// The loop exits when next_emit falls outside [ring_t[1], ring_t[2]].
//
// t must be i64; v must be double.
void emit_resampler_hermite(IrEmissionContext& ec, std::size_t state_offset,
                             std::int64_t interval,
                             llvm::Value* t, llvm::Value* v,
                             ResamplerEmitCallback emit);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_RESAMPLER_H
