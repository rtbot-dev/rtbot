#ifndef RTBOT_JIT_EMIT_GATE_H
#define RTBOT_JIT_EMIT_GATE_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// GATE emitter — predicate gate that suppresses the entire program output
// for the current tick when the predicate is zero. Used for SQL WHERE-clause
// filtering.
//
// Mirrors FE GATE bytecode opcode (FusedScalarEval.h case 44).
//
// `should_emit_alloca` is an i1* alloca maintained by the SegmentEmitter at
// function entry, initialized to true. This emitter ANDs the alloca with
// (predicate != 0.0) — once GATE fires false, should_emit stays false until
// the function returns.
//
// At the end of the program, the SegmentEmitter branches on this alloca:
// if false, jump to ret_false instead of writing outputs.
void emit_gate(IrEmissionContext& ec,
               llvm::Value* should_emit_alloca,  // i1*
               llvm::Value* predicate);          // double

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_GATE_H
