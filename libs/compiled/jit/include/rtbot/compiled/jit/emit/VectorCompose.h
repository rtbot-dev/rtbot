#ifndef RTBOT_JIT_EMIT_VECTOR_COMPOSE_H
#define RTBOT_JIT_EMIT_VECTOR_COMPOSE_H

#include <cstddef>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/Join.h"

namespace rtbot::jit::emit {

// VectorCompose state layout, push, and try_sync are identical to Join's.
// The only difference is downstream: the synced N values become a width-N
// vector on the single output port, written as N consecutive output slots
// by the SegmentEmitter rather than being routed back through value_map.
//
// Push (t, v) onto port `port` of a VectorCompose at the given state offset.
inline void emit_vector_compose_push(IrEmissionContext& ec,
                                      std::size_t state_offset,
                                      std::size_t N, std::size_t port,
                                      llvm::Value* t, llvm::Value* v) {
  emit_join_push(ec, state_offset, N, port, t, v);
}

// Try to synchronize the N port queues. Returns the same shape as Join's
// try_sync: sync_flag + (out_t, out_vs[N]). On success, the SegmentEmitter
// writes out_vs[0..N) to consecutive output slots.
inline JoinSyncOutput emit_vector_compose_try_sync(IrEmissionContext& ec,
                                                    std::size_t state_offset,
                                                    std::size_t N) {
  return emit_join_try_sync(ec, state_offset, N);
}

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_VECTOR_COMPOSE_H
