#ifndef RTBOT_JIT_EMIT_ARITHMETIC_H
#define RTBOT_JIT_EMIT_ARITHMETIC_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Stateless 2->1 arithmetic. a OP b. Caller is responsible for upstream alignment.
llvm::Value* emit_add(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_sub(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_mul(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);
llvm::Value* emit_div(IrEmissionContext& ec, llvm::Value* a, llvm::Value* b);

// Stateless 1->1: v * k where k is a compile-time constant.
llvm::Value* emit_scale(IrEmissionContext& ec, llvm::Value* v, double k);

// Stateless 1->1: v + k where k is a compile-time constant (FE Add scalar).
llvm::Value* emit_add_scalar(IrEmissionContext& ec, llvm::Value* v, double k);

// Stateless 1->1: pow(v, k) where k is a compile-time constant (FE Power scalar).
llvm::Value* emit_power_scalar(IrEmissionContext& ec, llvm::Value* v, double k);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_ARITHMETIC_H
