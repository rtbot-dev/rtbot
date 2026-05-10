#ifndef RTBOT_JIT_EMIT_STDDEV_H
#define RTBOT_JIT_EMIT_STDDEV_H

#include <cstddef>
#include <cstdint>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // for StatefulOutput

namespace rtbot::jit::emit {

// Emit IR for one StdDev<W> step at the given state offset.
//
// State layout (W+3 doubles at state_offset) — same ring+Kahan layout as MA:
//   [state_offset .. state_offset+W-1]  ring buffer
//   [state_offset+W]                    Kahan sum
//   [state_offset+W+1]                  Kahan compensation
//   [state_offset+W+2]                  count (double)
//
// Returns the (out_t, out_v, emit_flag) triple. out_v is the sample standard
// deviation sqrt(sum((xi-mean)^2) / (W-1)), matching StdDevStage<W>::process.
//
// t must be i64; v must be double.
StatefulOutput emit_stddev(IrEmissionContext& ec, std::size_t state_offset,
                           std::size_t W,
                           llvm::Value* t, llvm::Value* v);

// Result of the update-only path: state updated, flag computed, no PHI nodes.
struct SDUpdateResult {
  llvm::Value* emit_flag;    // i1 — count_new >= W
  llvm::Value* ring_base;    // double* — GEP to ring start
  llvm::Value* sum_ptr;      // double* — GEP to Kahan sum slot
  llvm::Value* count_new;    // double — post-increment count
  llvm::Value* pre_idx;      // i64 — pre_count % W (ring write index)
  llvm::Value* w_d;          // double constant W
  llvm::Value* w_u64;        // i64 constant W
};

// Update SD state (ring push + Kahan + count++) without branching on warmup.
// All IR is straight-line in the caller's current block. No basic blocks created.
SDUpdateResult emit_stddev_update(IrEmissionContext& ec,
                                  std::size_t state_offset,
                                  std::size_t W,
                                  llvm::Value* v);

// Compute SD output value (variance loop + sqrt) inline. Creates a loop
// (sdo_var_hdr / sdo_var_body / sdo_var_done basic blocks) but no warmup branches.
// Call this in the emit block after the combined warmup guard.
// W must match the W passed to emit_stddev_update.
llvm::Value* emit_stddev_output(IrEmissionContext& ec, const SDUpdateResult& upd,
                                std::size_t W);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_STDDEV_H
