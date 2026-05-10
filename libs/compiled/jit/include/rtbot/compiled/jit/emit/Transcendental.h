#ifndef RTBOT_JIT_EMIT_TRANSCENDENTAL_H
#define RTBOT_JIT_EMIT_TRANSCENDENTAL_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Transcendental, rounding, and sign emitters.
//
// All intrinsic-backed functions use llvm::Intrinsic::getDeclaration so the
// JIT back-end lowers them to the same libm call sites that FusedScalarEval's
// std:: overloads resolve to, giving bit-exact parity under the same FP env.
//
// emit_tan has no direct LLVM intrinsic — it is lowered as sin(v) / cos(v),
// which on the codegen path resolves via the same libm tan as the FE
// interpreter when fast-math is disabled (the IrEmissionContext contract).

llvm::Value* emit_pow(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_abs(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_sqrt(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_log(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_log10(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_exp(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_sin(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_cos(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_tan(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_sign(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_floor(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_ceil(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_round(IrEmissionContext& ec, llvm::Value* v);
llvm::Value* emit_neg(IrEmissionContext& ec, llvm::Value* v);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_TRANSCENDENTAL_H
