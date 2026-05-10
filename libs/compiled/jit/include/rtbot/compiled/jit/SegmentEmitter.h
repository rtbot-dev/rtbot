#ifndef RTBOT_JIT_SEGMENT_EMITTER_H
#define RTBOT_JIT_SEGMENT_EMITTER_H

#include <cstddef>
#include <string>
#include <vector>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "rtbot/compiled/jit/CompiledGraph.h"
#include "rtbot/compiled/jit/SegmentPartitioner.h"
#include "rtbot/compiled/jit/StateLayout.h"

namespace rtbot::jit {

// Information about the JIT'd function for a segment.
struct EmittedSegment {
  // Function name in the module — caller looks this up via JIT.
  std::string function_name;
  // Number of output values per emitted tick (the size of the
  // `out_v_array` parameter the function expects).
  std::size_t num_outputs;
  // Total state buffer size in doubles for this segment.
  std::size_t state_size_doubles;
};

// Emit IR for a segment. Adds one function to `mod` with signature
//
//   extern "C" int32_t segment_process(double* state, int64_t t, double v,
//                                       int64_t* out_t_arr,
//                                       double* out_v_arr,
//                                       int32_t* out_port_id_arr);
//
// The function returns the number of records written. Each record fills slot
// `r` in out_t_arr, out_port_id_arr, and out_v_arr[r * num_outputs ..].
// out_port_id_arr defaults to 0; Demux/Mux populate it. `out_v_arr` must be
// sized at least `K * num_outputs` where K is the program's max emits per call.
EmittedSegment emit_segment(llvm::LLVMContext& ctx, llvm::Module& mod,
                            const CompiledGraph& graph,
                            const Segment& segment,
                            const StateLayout& layout);

// Emit IR for a full multi-segment graph containing Join sync operators.
//
// Emits a single fused function spanning all linear segments + the Join sync
// logic between them. The function signature is identical to emit_segment:
//
//   extern "C" int32_t program_process(double* state, int64_t t, double v,
//                                       int64_t* out_t_arr,
//                                       double* out_v_arr,
//                                       int32_t* out_port_id_arr);
//
// Only Join sync ops are supported. Graphs containing Demux, Mux, or Pipeline
// ops will throw std::runtime_error. Graphs that also contain a ResamplerHermite
// combined with Joins are rejected (handle with emit_segment instead).
//
// The emitted control flow follows the PPGCompiled pattern: each op runs in
// topological order; stateful ops (MA, StdDev, PeakDetector) run unconditionally
// for state-update purposes, and their downstream sub-paths are gated on the
// op's emit_flag; Join try_sync is called unconditionally and its downstream
// sub-path is gated on the sync flag. All paths converge at a final try_sync
// of the output-side Join, which, if it fires, writes the outputs and returns true.
EmittedSegment emit_program(llvm::LLVMContext& ctx, llvm::Module& mod,
                            const CompiledGraph& graph,
                            const StateLayout& layout);

// Variant that takes the program-level Input's lane width. When > 1, the
// emitted function signature uses (state, t, const double* in_v_arr, ...)
// instead of (state, t, double v, ...). Used for top-level vector-input
// programs (e.g. when the Input op feeds a width-N vector wire).
EmittedSegment emit_program_with_input_width(llvm::LLVMContext& ctx,
                                              llvm::Module& mod,
                                              const CompiledGraph& graph,
                                              const StateLayout& layout,
                                              std::size_t input_lane_width_N);

// Emit a JIT function for the inner program of a Pipeline node. The function
// has a stable name (`fn_name`) so the outer Pipeline IR (D3) can call it via
// llvm::Module::getFunction.
//
// Function signature (extern "C"):
//   int32_t fn(double* inner_state,
//              int64_t  t,
//              const double* in_v_arr,    // length = sum of input port widths
//              int64_t* out_t_arr,        // length = K
//              double*  out_v_arr,        // length = K * outer_num_outputs
//              int32_t* out_port_id_arr); // length = K
//
// Returns the number of records written (0..K).
//
// `inner` is the Pipeline OpNode's inner CompiledGraph. It does NOT contain
// top-level Input/Output adapter ops; the entry op consumes inputs directly
// via `inner.entry_op_id`, and emissions are routed externally via
// `outer_output_mappings` (inner_op_id + inner_port -> outer pipeline port).
//
// `outer_input_port_types` declares the layout of `in_v_arr` (one slot per
// scalar input port; in segment-bytecode mode a single VECTOR_NUMBER port
// occupies `outer_input_lane_width` consecutive slots).
//
// `outer_output_mappings` is the Pipeline OpNode's pipeline_output_mappings:
// each entry maps (outer_port_idx, (inner_op_id, inner_port)).
//
// `outer_num_outputs` is the total scalar-slot count for the Pipeline output
// (currently equal to the Pipeline output port count, since today's Pipeline
// only supports scalar output ports).
//
// `max_emits_per_call_K` is the program-wide K used to size out_*_arr.
//
// `outer_input_lane_width` is the lane count when the input is a VECTOR_NUMBER
// (segment-bytecode mode); 0 means "scalar input" (the historical shape).
//
// The implementation supports two input shapes: single-port scalar input
// (control-port Pipelines) and single-port VECTOR_NUMBER input of width
// `outer_input_lane_width` (segment-bytecode Pipelines). Multi-port scalar
// inputs are not yet supported.
llvm::Function* emit_inner_program(
    llvm::LLVMContext& ctx, llvm::Module& mod,
    const CompiledGraph& inner,
    const std::string& fn_name,
    const std::vector<std::string>& outer_input_port_types,
    const std::vector<std::pair<std::size_t, std::pair<std::string, std::size_t>>>&
        outer_output_mappings,
    std::size_t outer_num_outputs,
    std::size_t max_emits_per_call_K,
    std::size_t outer_input_lane_width = 0,
    // If non-empty, the inner graph contains a prototype-declared Input op
    // with this id; emit_inner_program rewrites every `from=prototype_input_id`
    // connection to point at the synthetic Input it injects, then drops the
    // prototype Input node from the transformed graph. Used by KeyedPipeline
    // whose FE prototype carries its own Input op.
    const std::string& prototype_input_id = std::string{});

}  // namespace rtbot::jit

#endif  // RTBOT_JIT_SEGMENT_EMITTER_H
