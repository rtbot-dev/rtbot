#ifndef RTBOT_JIT_EMIT_PEAK_DETECTOR_H
#define RTBOT_JIT_EMIT_PEAK_DETECTOR_H

#include <cstddef>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // for StatefulOutput

namespace rtbot::jit::emit {

// Stateful 1->1 emitter for PeakDetector<W>.
//
// Mirrors PeakDetectorStage<W>::process from
// libs/compiled/include/rtbot/compiled/PeakDetectorStage.h:
//   1. Push (t, v) to ring at idx = count % W
//   2. ++count
//   3. If count < W: emit_flag = false (warmup)
//   4. Else: compute center_idx = (idx + 1 + W/2) % W
//      walk W elements; emit if center is strictly greater than all others
//      out_t = ring_t[center_idx] (bitcast double-as-i64), out_v = ring_v[center_idx]
//
// State layout (2*W + 1 doubles at state_offset):
//   [state_offset .. state_offset+W-1]   ring_v (values)
//   [state_offset+W .. state_offset+2W-1] ring_t (timestamps bitcast to double)
//   [state_offset+2W]                    count (as double)
//
// W must be odd and >= 3 (enforced by the AOT static_assert; JIT trusts the caller).
//
// t must be i64; v must be double.
StatefulOutput emit_peak_detector(IrEmissionContext& ec, std::size_t state_offset,
                                   std::size_t W,
                                   llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_PEAK_DETECTOR_H
