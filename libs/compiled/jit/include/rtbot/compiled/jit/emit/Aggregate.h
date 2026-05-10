#ifndef RTBOT_JIT_EMIT_AGGREGATE_H
#define RTBOT_JIT_EMIT_AGGREGATE_H

#include <cstddef>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Stateless-output (always emits) aggregate emitters. All return the new
// running accumulator value as a double Value*.
//
// State layouts:
//   CumSum: 2 doubles (sum, comp) — Kahan-compensated.
//   Count:  1 double (count).
//   MaxAgg: 1 double (max). Slot must be init to -inf via state_init_overrides.
//   MinAgg: 1 double (min). Slot must be init to +inf via state_init_overrides.

llvm::Value* emit_cumsum(IrEmissionContext& ec, std::size_t state_offset, llvm::Value* v);
llvm::Value* emit_count(IrEmissionContext& ec, std::size_t state_offset);
llvm::Value* emit_max_agg(IrEmissionContext& ec, std::size_t state_offset, llvm::Value* v);
llvm::Value* emit_min_agg(IrEmissionContext& ec, std::size_t state_offset, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_AGGREGATE_H
