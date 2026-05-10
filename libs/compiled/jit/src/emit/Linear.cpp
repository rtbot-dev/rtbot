// Linear.cpp
//
// IR emission for Linear (N-port sync + Σ c_i * v_i). Reuses the shared
// emit_join_try_sync to drive the N-port synchronization machinery, then
// folds the synced values with a sequential left-to-right sum-of-products
// matching FE Linear::process_data exactly.

#include "rtbot/compiled/jit/emit/Linear.h"

#include <stdexcept>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

#include "rtbot/compiled/jit/emit/Join.h"

namespace rtbot::jit::emit {

ReducedSyncOutput emit_linear_try_sync(IrEmissionContext& ec,
                                       std::size_t state_offset,
                                       const std::vector<double>& coeffs) {
  if (coeffs.size() < 2) {
    throw std::runtime_error(
        "emit_linear_try_sync: at least 2 coefficients required");
  }
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);

  // Run the shared N-port sync. After this returns, the IRBuilder is in the
  // try_sync exit block; sync_flag/out_t/out_vs are PHIs valid in that block.
  JoinSyncOutput sync = emit_join_try_sync(ec, state_offset, coeffs.size());

  // FE Linear::process_data computes:
  //   result = 0.0
  //   for i in 0..N-1: result += coeffs[i] * msgs[i].value
  // No FastMath; the IRBuilder is configured with empty FastMathFlags.
  // We emit the same FAdd/FMul sequence regardless of sync_flag — the
  // result is only consumed when sync_flag is true. Emitting unconditionally
  // is correct because the values are PHI'd to 0.0 in the no-sync path,
  // which keeps the IR straight-line and matches Join's pattern.
  llvm::Value* acc = llvm::ConstantFP::get(f64, 0.0);
  for (std::size_t i = 0; i < coeffs.size(); ++i) {
    llvm::Value* ci = llvm::ConstantFP::get(f64, coeffs[i]);
    llvm::Value* term = b.CreateFMul(ci, sync.out_vs[i], "lin_term");
    acc = b.CreateFAdd(acc, term, "lin_acc");
  }

  return ReducedSyncOutput{sync.sync_flag, sync.out_t, acc};
}

}  // namespace rtbot::jit::emit
