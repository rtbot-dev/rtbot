#ifndef RTBOT_JIT_EMIT_MUX_H
#define RTBOT_JIT_EMIT_MUX_H

#include <cstddef>
#include <vector>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Push (t, v) onto data port `port` (0-based) of a Multiplexer<N>.
// Internally maps to emit_join_push at (state_offset, port=port).
void emit_mux_push_data(IrEmissionContext& ec, std::size_t state_offset,
                        std::size_t N, std::size_t port,
                        llvm::Value* t, llvm::Value* v);

// Push (t, v) onto control port `ctrl_idx` (0-based) of a Multiplexer<N>.
// Internally maps to emit_join_push at (state_offset, port=N+ctrl_idx).
void emit_mux_push_control(IrEmissionContext& ec, std::size_t state_offset,
                           std::size_t N, std::size_t ctrl_idx,
                           llvm::Value* t, llvm::Value* v);

// Output of one try_sync attempt for a Multiplexer.
//   sync_flag: i1 — emit_flag, true when exactly one control voted true and
//                    the corresponding data port matched the control time.
//   out_t:     i64 timestamp.
//   out_v:     double value forwarded from the matching data port.
struct MuxTrySyncOutput {
  llvm::Value* sync_flag;
  llvm::Value* out_t;
  llvm::Value* out_v;
};

// Try to perform one Multiplexer dispatch step. Mirrors the FE
// Multiplexer::process_data inner loop, including:
//   - sync controls (drop mismatched-older fronts)
//   - find_port_to_emit: exactly one control true at the synced time
//   - if matching data port front time == ctrl time, emit; pop matching data
//     and all controls
//   - if no unique port, drop all data fronts at time <= ctrl time and pop
//     all controls
MuxTrySyncOutput emit_mux_try_sync(IrEmissionContext& ec,
                                    std::size_t state_offset,
                                    std::size_t N);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_MUX_H
