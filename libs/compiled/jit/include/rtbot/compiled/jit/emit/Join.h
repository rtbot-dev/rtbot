#ifndef RTBOT_JIT_EMIT_JOIN_H
#define RTBOT_JIT_EMIT_JOIN_H

#include <cstddef>
#include <vector>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/CompiledGraph.h"
#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Push (t, v) onto port `port` of a Join<N> at the given state offset.
// Updates head/size; if size was already `cap`, drops the oldest entry
// (advances head, decrements size before the write).
//
// `cap` must match the capacity baked into the state layout (2*cap+2 doubles
// per port). Mirrors PortBuffer<cap>::push from JoinStage.h.
void emit_join_push(IrEmissionContext& ec, std::size_t state_offset, std::size_t N,
                    std::size_t port, llvm::Value* t, llvm::Value* v,
                    std::size_t cap = kJitDefaultSyncQueueCapacity);

// Try to synchronize the N port queues. If all ports have a front entry at
// the same timestamp, returns sync_flag=true plus the synced (out_t, out_vs).
// Pops one entry from each port on success. Older mismatched fronts are
// discarded inside the function.
//
// `cap` must match the capacity baked into the state layout. The loop bound
// is 2*cap iterations. Mirrors JoinStage<N>::try_sync.
struct JoinSyncOutput {
  llvm::Value* sync_flag;            // i1
  llvm::Value* out_t;                // i64
  std::vector<llvm::Value*> out_vs;  // N doubles
};

JoinSyncOutput emit_join_try_sync(IrEmissionContext& ec, std::size_t state_offset,
                                  std::size_t N,
                                  std::size_t cap = kJitDefaultSyncQueueCapacity);

// ---------------------------------------------------------------------------
// Per-port buffer helpers shared with Demultiplexer / Multiplexer emitters.
// Each port occupies cap * 2 + 2 doubles.
// `cap` must match the capacity baked into the state layout.
// ---------------------------------------------------------------------------

// Pop the front element from port `port` (advance head, decrement size).
void emit_port_pop_front(IrEmissionContext& ec, std::size_t state_offset,
                         std::size_t port,
                         std::size_t cap = kJitDefaultSyncQueueCapacity);

// Load size of port `port` as i64.
llvm::Value* emit_port_size(IrEmissionContext& ec, std::size_t state_offset,
                            std::size_t port,
                            std::size_t cap = kJitDefaultSyncQueueCapacity);

// Load front timestamp (i64) of port `port`.
llvm::Value* emit_port_front_time(IrEmissionContext& ec, std::size_t state_offset,
                                  std::size_t port,
                                  std::size_t cap = kJitDefaultSyncQueueCapacity);

// Load front value (double) of port `port`.
llvm::Value* emit_port_front_value(IrEmissionContext& ec, std::size_t state_offset,
                                   std::size_t port,
                                   std::size_t cap = kJitDefaultSyncQueueCapacity);

// Compute the per-port stride in doubles for a given capacity.
inline constexpr std::size_t join_port_stride(std::size_t cap) {
  return 2 * cap + 2;
}

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_JOIN_H
