#ifndef RTBOT_JIT_EMIT_RESAMPLER_CONSTANT_H
#define RTBOT_JIT_EMIT_RESAMPLER_CONSTANT_H

#include <cstddef>
#include <cstdint>
#include <functional>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Callback invoked for each emission inside the resampler step.
// The IRBuilder is positioned in the call site when invoked.
// (out_t: i64, out_v: double)
using ResamplerConstantEmitCallback =
    std::function<void(llvm::Value* /*out_t*/, llvm::Value* /*out_v*/)>;

// Emit IR for one ResamplerConstant step at the given state offset.
//
// State layout (3 doubles at state_offset):
//   [0] last_value   — most recent input value (zero-order-hold)
//   [1] next_emit    — next emission timestamp (bit-cast i64 <-> double)
//   [2] initialized  — 0.0=false / 1.0=true
//
// Parameters baked in IR:
//   interval  — fixed grid spacing (i64 dt, must be > 0)
//   t0_set    — if true, anchor grid to t0; otherwise grid starts at t+dt
//   t0        — only used when t0_set
//   snap_first— if true, on init use the floor-aligned grid point and emit
//
// The callback is invoked 0..N times per step; IRBuilder stays positioned at
// the post-call block on exit.
//
// t must be i64; v must be double.
void emit_resampler_constant(IrEmissionContext& ec, std::size_t state_offset,
                              std::int64_t interval, bool t0_set,
                              std::int64_t t0, bool snap_first,
                              llvm::Value* t, llvm::Value* v,
                              ResamplerConstantEmitCallback emit_cb);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_RESAMPLER_CONSTANT_H
