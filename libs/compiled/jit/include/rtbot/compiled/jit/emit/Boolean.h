#ifndef RTBOT_JIT_EMIT_BOOLEAN_H
#define RTBOT_JIT_EMIT_BOOLEAN_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Stateless boolean emitters. Treat any non-zero double as true, zero as false.
// Outputs are 1.0 (true) or 0.0 (false), matching the FE interpreter semantics.
// Ordered IEEE-754 FCmp variants are used — NaN inputs yield false.
llvm::Value* emit_and(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_or(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_not(IrEmissionContext& ec, llvm::Value* v);

// Extra BooleanSync variants. Same 0.0/non-zero input convention; 0.0/1.0 output.
llvm::Value* emit_xor(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_nand(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_nor(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_xnor(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_implication(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_BOOLEAN_H
