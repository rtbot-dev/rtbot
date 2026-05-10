#include "rtbot/compiled/jit/emit/Filter.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

#include "rtbot/compiled/jit/emit/Transcendental.h"

namespace rtbot::jit::emit {

namespace {

llvm::Value* dconst(llvm::IRBuilder<>& bld, double k) {
  return llvm::ConstantFP::get(bld.getDoubleTy(), k);
}

}  // namespace

StatefulOutput emit_filt_gt_scalar(IrEmissionContext& ec, double k,
                                    llvm::Value* t, llvm::Value* v) {
  auto& bld = ec.b();
  llvm::Value* flag = bld.CreateFCmpOGT(v, dconst(bld, k));
  return StatefulOutput{t, v, flag};
}

StatefulOutput emit_filt_lt_scalar(IrEmissionContext& ec, double k,
                                    llvm::Value* t, llvm::Value* v) {
  auto& bld = ec.b();
  llvm::Value* flag = bld.CreateFCmpOLT(v, dconst(bld, k));
  return StatefulOutput{t, v, flag};
}

StatefulOutput emit_filt_eq_scalar(IrEmissionContext& ec, double target,
                                    double epsilon,
                                    llvm::Value* t, llvm::Value* v) {
  auto& bld = ec.b();
  llvm::Value* diff  = bld.CreateFSub(v, dconst(bld, target));
  llvm::Value* abs_d = emit_abs(ec, diff);
  llvm::Value* flag  = bld.CreateFCmpOLE(abs_d, dconst(bld, epsilon));
  return StatefulOutput{t, v, flag};
}

StatefulOutput emit_filt_neq_scalar(IrEmissionContext& ec, double target,
                                     double epsilon,
                                     llvm::Value* t, llvm::Value* v) {
  auto& bld = ec.b();
  llvm::Value* diff  = bld.CreateFSub(v, dconst(bld, target));
  llvm::Value* abs_d = emit_abs(ec, diff);
  llvm::Value* flag  = bld.CreateFCmpOGT(abs_d, dconst(bld, epsilon));
  return StatefulOutput{t, v, flag};
}

StatefulOutput emit_filt_gt_sync(IrEmissionContext& ec,
                                  llvm::Value* t,
                                  llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* flag = bld.CreateFCmpOGT(a, b);
  return StatefulOutput{t, a, flag};
}

StatefulOutput emit_filt_lt_sync(IrEmissionContext& ec,
                                  llvm::Value* t,
                                  llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* flag = bld.CreateFCmpOLT(a, b);
  return StatefulOutput{t, a, flag};
}

StatefulOutput emit_filt_eq_sync(IrEmissionContext& ec, double epsilon,
                                  llvm::Value* t,
                                  llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* diff  = bld.CreateFSub(a, b);
  llvm::Value* abs_d = emit_abs(ec, diff);
  // FE SyncEqual: filter_condition is |a - b| < epsilon (strict).
  llvm::Value* flag  = bld.CreateFCmpOLT(abs_d, dconst(bld, epsilon));
  return StatefulOutput{t, a, flag};
}

StatefulOutput emit_filt_neq_sync(IrEmissionContext& ec, double epsilon,
                                   llvm::Value* t,
                                   llvm::Value* a, llvm::Value* b) {
  auto& bld = ec.b();
  llvm::Value* diff  = bld.CreateFSub(a, b);
  llvm::Value* abs_d = emit_abs(ec, diff);
  // FE SyncNotEqual: |a - b| >= epsilon.
  llvm::Value* flag  = bld.CreateFCmpOGE(abs_d, dconst(bld, epsilon));
  return StatefulOutput{t, a, flag};
}

}  // namespace rtbot::jit::emit
