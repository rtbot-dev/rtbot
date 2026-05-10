#ifndef RTBOT_JIT_EMIT_DEMUX_H
#define RTBOT_JIT_EMIT_DEMUX_H

#include <cstddef>
#include <vector>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Push (t, v) onto the data port of a Demultiplexer<N> at state_offset.
// Internally maps to emit_join_push at (state_offset, port=0).
void emit_demux_push_data(IrEmissionContext& ec, std::size_t state_offset,
                          llvm::Value* t, llvm::Value* v);

// Push (t, v) onto control port `ctrl_idx` (0-based) of a Demultiplexer<N>.
// Internally maps to emit_join_push at (state_offset, port=1+ctrl_idx).
void emit_demux_push_control(IrEmissionContext& ec, std::size_t state_offset,
                             std::size_t N, std::size_t ctrl_idx,
                             llvm::Value* t, llvm::Value* v);

// Output of a single try_sync attempt. For each output port p in [0, N) the
// caller writes (out_t, port_values[p]) to slot record_idx if port_emit[p] is
// true, then bumps record_idx. The returned record_count is the number of
// output ports that fired (0..N).
struct DemuxTrySyncOutput {
  llvm::Value* sync_flag;                  // i1 — controls aligned & data front matched
  llvm::Value* out_t;                      // i64 timestamp common to all emits
  std::vector<llvm::Value*> port_emit;     // i1 per output port
  std::vector<llvm::Value*> port_values;   // double per output port
};

// Try to perform one Demultiplexer dispatch step. Mirrors the FE
// Demultiplexer::process_data inner loop:
//   1. Sync all N control queues (drop older mismatched fronts).
//   2. If data front time == ctrl front time: pop everything; for each ctrl
//      port if value is true, mark that output port as emitting.
//   3. Otherwise drop the older queue front and signal sync_flag=false.
DemuxTrySyncOutput emit_demux_try_sync(IrEmissionContext& ec,
                                       std::size_t state_offset,
                                       std::size_t N);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_DEMUX_H
