#ifndef RTBOT_JIT_EMIT_COMPARISON_H
#define RTBOT_JIT_EMIT_COMPARISON_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Stateless 2->1 comparison. a OP b. Returns 1.0 (true) or 0.0 (false).
// Uses ordered IEEE-754 FCmp variants — NaN inputs yield 0.0, matching the
// FE interpreter's (a OP b) ? 1.0 : 0.0 semantics under -fno-fast-math.
llvm::Value* emit_gt(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_gte(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_lt(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_lte(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_eq(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_neq(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);

// Tolerant equality: |a - b| <= tol → 1.0 else 0.0 (matches FE CompareSyncEQ).
llvm::Value* emit_eq_tol(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b,
                          double tol);
// Tolerant inequality: |a - b| > tol → 1.0 else 0.0 (matches FE CompareSyncNEQ).
llvm::Value* emit_neq_tol(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b,
                           double tol);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_COMPARISON_H
