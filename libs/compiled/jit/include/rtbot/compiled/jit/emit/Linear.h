#ifndef RTBOT_JIT_EMIT_LINEAR_H
#define RTBOT_JIT_EMIT_LINEAR_H

#include <cstddef>
#include <vector>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// Output of emit_linear_try_sync / emit_reduce_join_try_sync.
// emit_flag: i1, true when the N-port sync produced a synced tuple AND the
//            fold produced a value (e.g. Division does not emit on /0).
// out_t: i64 timestamp (valid only when emit_flag is true).
// out_v: f64 reduced value (valid only when emit_flag is true).
struct ReducedSyncOutput {
  llvm::Value* emit_flag;
  llvm::Value* out_t;
  llvm::Value* out_v;
};

// Emit Linear's N-port sync + linear-combination fold:
//   if all ports synced at the same t:
//     result = 0.0
//     for i in 0..N-1: result += coeffs[i] * v_i
//     emit (t, result)
//
// FP order matches FE Linear::process_data exactly (sequential left-fold,
// no FastMath). N is implied by coeffs.size() (must be >= 2).
ReducedSyncOutput emit_linear_try_sync(IrEmissionContext& ec,
                                       std::size_t state_offset,
                                       const std::vector<double>& coeffs);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_LINEAR_H
