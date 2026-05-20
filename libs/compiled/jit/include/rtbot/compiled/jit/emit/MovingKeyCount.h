#ifndef RTBOT_JIT_EMIT_MOVING_KEY_COUNT_H
#define RTBOT_JIT_EMIT_MOVING_KEY_COUNT_H

#include <cstddef>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // StatefulOutput

namespace rtbot::jit::emit {

// Stateful 1->1 emitter for MovingKeyCount<W>. Always emits (no warmup).
//
// State layout (W + 3 doubles starting at state_offset):
//   [0..W-1]  ring buffer of recent keys
//   [W]       ring_count (number of valid entries, max = W)
//   [W+1]     ring_head (next insert index, 0..W-1)
//   [W+2]     ctx_ptr (bit-cast pointer to MovingKeyCountNodeCtx)
//
// Per tick:
//   1. Load ring_count, ring_head, ctx_ptr.
//   2. evict_valid = (ring_count == W).
//   3. evicted_key = ring[ring_head] if evict_valid else 0.0.
//   4. Store new key into ring[ring_head].
//   5. ring_head = (ring_head + 1) % W.
//   6. ring_count = min(ring_count + 1, W).
//   7. count = rtbot_jit_mkc_step(ctx, new_key, evicted_key, evict_valid).
//   8. Emit (t, count) — emit_flag is always true.
StatefulOutput emit_moving_key_count(IrEmissionContext& ec,
                                     std::size_t state_offset, std::size_t W,
                                     llvm::Value* t, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_MOVING_KEY_COUNT_H
