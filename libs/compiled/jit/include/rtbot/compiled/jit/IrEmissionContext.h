#ifndef RTBOT_JIT_IR_EMISSION_CONTEXT_H
#define RTBOT_JIT_IR_EMISSION_CONTEXT_H

#include <cstddef>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace rtbot::jit {

// Shared context handed to each per-opcode emitter. Holds the LLVM module,
// the IRBuilder positioned at the current insertion point, and the state
// pointer that all stateful opcodes index into for their own slots.
//
// The state pointer points to a flat double[] buffer that the
// StateLayout planner has sized and per-opcode-offsets assigned. Stateful
// emitters use state_gep(offset) to compute their own slot pointers.
//
// FP-determinism contract: the IRBuilder is always configured with empty
// FastMathFlags. Don't change them.
class IrEmissionContext {
 public:
  IrEmissionContext(llvm::LLVMContext& ctx, llvm::Module& mod,
                    llvm::IRBuilder<>& builder, llvm::Value* state_ptr);

  // GEP into the state buffer at an absolute offset (in doubles).
  // Returns a double* pointing into the state buffer at the given slot.
  llvm::Value* state_gep(std::size_t double_offset);

  // Common Kahan sliding-sum subtract step. value_to_remove is being
  // removed from the running sum; sum_ptr and comp_ptr are GEPs into
  // the state buffer at the running sum/compensation slots.
  //
  // Mirrors the C++ pattern:
  //   double ys = (-leaving) - comp;
  //   double ts = sum + ys;
  //   comp = (ts - sum) - ys;
  //   sum = ts;
  void emit_kahan_subtract(llvm::Value* sum_ptr, llvm::Value* comp_ptr,
                           llvm::Value* value_to_remove);

  // Common Kahan sliding-sum add step. value_to_add is being added.
  //
  // Mirrors:
  //   double ya = v - comp;
  //   double ta = sum + ya;
  //   comp = (ta - sum) - ya;
  //   sum = ta;
  void emit_kahan_add(llvm::Value* sum_ptr, llvm::Value* comp_ptr,
                      llvm::Value* value_to_add);

  // Hermite cubic interpolation — same formula as ResamplerHermiteStage,
  // verbatim with *(1+0.0)*(1-0.0)/2 form (not simplified to *0.5) for
  // FP-bit-exact parity. Returns the interpolated value.
  llvm::Value* emit_hermite_interp(llvm::Value* y0, llvm::Value* y1,
                                    llvm::Value* y2, llvm::Value* y3,
                                    llvm::Value* mu);

  // Accessors used by emitters.
  llvm::LLVMContext& ctx() { return ctx_; }
  llvm::Module& mod() { return mod_; }
  llvm::IRBuilder<>& b() { return b_; }
  llvm::Value* state_ptr() { return state_ptr_; }

 private:
  llvm::LLVMContext& ctx_;
  llvm::Module& mod_;
  llvm::IRBuilder<>& b_;
  llvm::Value* state_ptr_;
};

}  // namespace rtbot::jit

#endif  // RTBOT_JIT_IR_EMISSION_CONTEXT_H
