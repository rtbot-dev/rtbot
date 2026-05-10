#ifndef RTBOT_JIT_EMIT_TOPK_H
#define RTBOT_JIT_EMIT_TOPK_H

#include <cstddef>
#include <vector>

#include <llvm/IR/Value.h>

#include "rtbot/compiled/jit/IrEmissionContext.h"

namespace rtbot::jit::emit {

// IR emission for one TopK step. Mirrors FE TopK::insert_into_top_k +
// the per-row emit loop:
//
//   1. Find insertion position via lower_bound:
//        descending: first j where stored_score[j] <= new_score
//        ascending : first j where stored_score[j] >= new_score
//   2. Shift rows [j..count) right by one (clamp at K-1 to avoid the
//      extra row at slot K, which we drop anyway).
//   3. Write the new row at slot j.
//   4. Bump count: count = min(count + 1, K).
//   5. For each j in [0, count): write a record into the caller-provided
//      out_t / out_v / out_port_id buffers (slot j), with out_t = t and
//      out_v[j*W + k] = sorted_row[j].lane[k]. Write port_id=0 for each
//      record. Return count.
//
// State layout (1 + K*W doubles starting at state_offset):
//   [0]                count_d  — current row occupancy in [0, K], stored as double
//   [1 + j*W + k]      lane k of row j (best at j=0, worst at j=count-1)
//
// `row_lanes` is the `width` of the input PortValue: 1 for scalar wires,
// or the vector width when the input is an SSA `[N x double]`. Caller
// passes the input as a vector of `row_lanes` SSA scalars (already
// extracted from any vector wire).
//
// out_port_id is the port id assigned to each emitted record (always the
// same since TopK emits all rows on its single output port).
//
// Returns the i32 count of records written (in [0, K]).
llvm::Value* emit_topk(IrEmissionContext& ec,
                       std::size_t state_offset,
                       std::size_t K,
                       std::size_t row_lanes,
                       std::size_t score_index,
                       bool descending,
                       llvm::Value* t,
                       const std::vector<llvm::Value*>& input_lanes,
                       llvm::Value* out_t_arr,
                       llvm::Value* out_v_arr,
                       llvm::Value* out_port_id_arr,
                       std::size_t  num_outputs_per_record,
                       std::size_t  out_port_id);

}  // namespace rtbot::jit::emit

#endif  // RTBOT_JIT_EMIT_TOPK_H
