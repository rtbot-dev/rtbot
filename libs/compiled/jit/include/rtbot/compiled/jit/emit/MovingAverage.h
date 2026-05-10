#ifndef RTBOT_JIT_EMIT_MOVING_AVERAGE_H
#define RTBOT_JIT_EMIT_MOVING_AVERAGE_H

#include <cstddef>
#include <cstdint>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

struct StatefulOutput {
  llvm::Value* out_t;     // i64 — emit timestamp
  llvm::Value* out_v;     // double — emit value
  llvm::Value* emit_flag; // i1 — true if a value should be emitted this tick
};

// Emit IR for one MovingAverage<W> step at the given state offset.
//
// State layout (W+3 doubles at state_offset):
//   [state_offset .. state_offset+W-1]  ring buffer
//   [state_offset+W]                    Kahan sum
//   [state_offset+W+1]                  Kahan compensation
//   [state_offset+W+2]                  count (double)
//
// Returns the (out_t, out_v, emit_flag) triple. The IRBuilder is left
// positioned in the block that follows both branch targets. The caller
// wraps the result in a conditional store/emit as needed.
//
// t must be i64; v must be double.
StatefulOutput emit_moving_average(IrEmissionContext& ec, std::size_t state_offset,
                                   std::size_t W,
                                   llvm::Value* t, llvm::Value* v);

// Result of the update-only path: state updated, flag computed, no PHI nodes.
// sum_ptr / count_ptr are GEPs into the state buffer; emit_flag is i1.
// Use emit_moving_average_output() in the emit block to get the actual value.
struct MAUpdateResult {
  llvm::Value* emit_flag;  // i1 — count_new >= W
  llvm::Value* sum_ptr;    // double* — GEP to Kahan sum slot
  llvm::Value* w_d;        // double constant W (convenience for output step)
};

// Update MA state (ring push + Kahan + count++) without branching on warmup.
// Returns MAUpdateResult. No basic blocks are created beyond the caller's
// current insertion block — all IR is straight-line.
// The IRBuilder remains in the same block on return.
MAUpdateResult emit_moving_average_update(IrEmissionContext& ec,
                                          std::size_t state_offset,
                                          std::size_t W,
                                          llvm::Value* v);

// Compute MA output value (sum / W) inline. Call this in the emit block after
// the warmup guard. No branches created.
llvm::Value* emit_moving_average_output(IrEmissionContext& ec,
                                        const MAUpdateResult& upd);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_MOVING_AVERAGE_H
