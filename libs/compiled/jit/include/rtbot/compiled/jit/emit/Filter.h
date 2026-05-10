#ifndef RTBOT_JIT_EMIT_FILTER_H
#define RTBOT_JIT_EMIT_FILTER_H

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"  // for StatefulOutput

namespace rtbot::jit::emit {

// FilterScalar 1-input predicate filters: emit (t, v) iff predicate(v, k) holds.
// All return StatefulOutput where emit_flag drives the SegmentEmitter's
// stateful-skip plumbing (out_t/out_v carry the input pair unchanged).

StatefulOutput emit_filt_gt_scalar(IrEmissionContext& ec, double k,
                                    llvm::Value* t, llvm::Value* v);
StatefulOutput emit_filt_lt_scalar(IrEmissionContext& ec, double k,
                                    llvm::Value* t, llvm::Value* v);

// EqualTo / NotEqualTo: |v - target| <(=) epsilon.
StatefulOutput emit_filt_eq_scalar(IrEmissionContext& ec, double target,
                                    double epsilon,
                                    llvm::Value* t, llvm::Value* v);
StatefulOutput emit_filt_neq_scalar(IrEmissionContext& ec, double target,
                                     double epsilon,
                                     llvm::Value* t, llvm::Value* v);

// FilterSync 2-input predicate filters: emit (t, a) iff predicate(a, b) holds.
// SyncGreaterThan/LessThan: a > b / a < b.
StatefulOutput emit_filt_gt_sync(IrEmissionContext& ec,
                                  llvm::Value* t,
                                  llvm::Value* a, llvm::Value* b);
StatefulOutput emit_filt_lt_sync(IrEmissionContext& ec,
                                  llvm::Value* t,
                                  llvm::Value* a, llvm::Value* b);

// SyncEqual: |a - b| < epsilon  (FE: filter_condition uses strict < epsilon).
StatefulOutput emit_filt_eq_sync(IrEmissionContext& ec, double epsilon,
                                  llvm::Value* t,
                                  llvm::Value* a, llvm::Value* b);
// SyncNotEqual: |a - b| >= epsilon.
StatefulOutput emit_filt_neq_sync(IrEmissionContext& ec, double epsilon,
                                   llvm::Value* t,
                                   llvm::Value* a, llvm::Value* b);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_FILTER_H
