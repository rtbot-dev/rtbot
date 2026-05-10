#include "rtbot/compiled/jit/emit/Replace.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

StatefulOutput emit_lte_replace(IrEmissionContext& ec, double threshold,
                                 double replace_by,
                                 llvm::Value* t, llvm::Value* v) {
  auto& b = ec.b();

  llvm::Type* f64 = b.getDoubleTy();

  // isfinite(v): true iff v is neither NaN nor infinite.
  // (v - v) == 0 is true exactly when v is finite (NaN/+-Inf produce NaN
  // which fails OEQ against 0.0).
  llvm::Value* zero    = llvm::ConstantFP::get(f64, 0.0);
  llvm::Value* diff    = b.CreateFSub(v, v, "rp_diff");
  llvm::Value* finite  = b.CreateFCmpOEQ(diff, zero, "rp_finite");

  // (v <= threshold) ? replace_by : v
  llvm::Value* thr_v   = llvm::ConstantFP::get(f64, threshold);
  llvm::Value* repl_v  = llvm::ConstantFP::get(f64, replace_by);
  llvm::Value* le      = b.CreateFCmpOLE(v, thr_v, "rp_le");
  llvm::Value* out_v   = b.CreateSelect(le, repl_v, v, "rp_out_v");

  return StatefulOutput{t, out_v, finite};
}

}  // namespace rtbot::jit::emit
