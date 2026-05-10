#ifndef RTBOT_JIT_EMIT_REDUCE_JOIN_H
#define RTBOT_JIT_EMIT_REDUCE_JOIN_H

#include <cstddef>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/CompiledGraph.h"
#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/Linear.h"  // ReducedSyncOutput

namespace rtbot::jit::emit {

// Emit ReduceJoin's N-port sync + per-element fold:
//   if all ports synced at the same t:
//     if has_init: acc = init_value, then for i in 0..N-1: acc = combine(acc, v_i)
//     else:        acc = v_0,        then for i in 1..N-1: acc = combine(acc, v_i)
//     emit (t, acc) (DivReduce suppresses emit when any divisor is exactly 0.0)
//
// Sequential left-fold matches the FE ReduceJoin<T>::process_data order
// exactly, so FAdd/FMul/FDiv/comparison ordering is bit-exact.
ReducedSyncOutput emit_reduce_join_try_sync(IrEmissionContext& ec,
                                            std::size_t state_offset,
                                            std::size_t n_ports,
                                            ReduceOp reduce_op);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_REDUCE_JOIN_H
