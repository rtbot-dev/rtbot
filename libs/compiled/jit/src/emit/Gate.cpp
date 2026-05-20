#include "rtbot/compiled/jit/emit/Gate.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

void emit_gate(IrEmissionContext& ec,
               llvm::Value* should_emit_alloca,
               llvm::Value* predicate) {
  auto& bld = ec.b();
  auto* dbl = bld.getDoubleTy();
  auto* zero_d = llvm::ConstantFP::get(dbl, 0.0);
  // pred != 0.0 → i1 true when predicate is non-zero
  auto* pred_nz = bld.CreateFCmpONE(predicate, zero_d);
  auto* prev = bld.CreateLoad(bld.getInt1Ty(), should_emit_alloca);
  auto* anded = bld.CreateAnd(prev, pred_nz);
  bld.CreateStore(anded, should_emit_alloca);
}

}  // namespace rtbot::jit::emit
