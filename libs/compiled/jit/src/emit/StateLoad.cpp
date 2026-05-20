// StateLoad.cpp
//
// IR emission for STATE_LOAD — read a named slot from the state buffer.
//
// STATE_LOAD does not own any state slot. It reads a slot written by a prior
// opcode in the same fused program (e.g. CUMSUM writes state[0]; STATE_LOAD 0
// re-reads that accumulated sum for a second output expression).
//
// Mirrors FE case 25:
//   stack[sp++] = state[i.arg];
//
// Implementation: one GEP + one load. No side effects on state.

#include "rtbot/compiled/jit/emit/StateLoad.h"

#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

llvm::Value* emit_state_load(IrEmissionContext& ec, std::size_t state_offset) {
  auto& b = ec.b();
  llvm::Type* f64 = llvm::Type::getDoubleTy(ec.ctx());
  llvm::Value* slot_ptr = ec.state_gep(state_offset);
  return b.CreateLoad(f64, slot_ptr, "sl_val");
}

}  // namespace rtbot::jit::emit
