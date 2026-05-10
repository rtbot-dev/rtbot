#ifndef RTBOT_JIT_COMPILED_GRAPH_H
#define RTBOT_JIT_COMPILED_GRAPH_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rtbot::jit {

struct CompiledGraph;

// Opcode tag — covers the FE opcode set + sync/routing.
// Values do not need to match the FE numeric opcodes; this is the JIT's
// internal representation only.
enum class OpKind {
  // Stateless arithmetic
  Add, Sub, Mul, Div, Scale,
  // Stateless arithmetic scalar (1-input + constant)
  AddScalar, PowerScalar,
  // Stateless transcendental
  Pow, Abs, Sqrt, Log, Log10, Exp, Sin, Cos, Tan, Sign, Floor, Ceil, Round, Neg,
  // Stateless comparison
  Gt, Gte, Lt, Lte, Eq, Neq,
  // Stateless comparison with tolerance (CompareSyncEQ/NEQ): |a - b| <= tol
  EqTol, NeqTol,
  // Stateless comparison scalar (CompareScalar: 1-input + constant)
  GtScalar, LtScalar, GteScalar, LteScalar, EqScalar, NeqScalar,
  // Stateless boolean
  And, Or, Not,
  // Stateless boolean (BooleanSync extras)
  Xor, Nand, Nor, Xnor, Implication,
  // Conditional emit: predicate filters (FilterScalar, 1-input + constant)
  FiltGtScalar, FiltLtScalar, FiltEqScalar, FiltNeqScalar,
  // Conditional emit: predicate filters (FilterSync, 2-input)
  FiltGtSync, FiltLtSync, FiltEqSync, FiltNeqSync,
  // Stateless trivial 1->1
  Identity, Constant, BooleanToNumber, TimeShift, TimestampExtract,
  // Stateless piecewise interpolator (linear or Hermite)
  Function,
  // Stateless conditional substitution
  LessThanOrEqualToReplace,
  // Stateful 1->1 (windowed)
  MovingAverage, MovingSum, StdDev, Diff, PeakDetector, SignChange, WinMin, WinMax, FIR, IIR,
  // Stateful 1->1 (unbounded aggregates — always emit)
  CumSum, Count, MaxAgg, MinAgg,
  // Stateful 1->1 keyed counter: ring buffer of last W keys + per-instance
  // hashmap counts. Always emits (no warmup). Backed by a runtime helper
  // (rtbot_jit_mkc_step) for the hashmap operations; ring buffer state lives
  // in the JIT's static state buffer.
  MovingKeyCount,
  // Stateful 1->many: emits up to K rows per input tick (warmup grows 1..K).
  // Terminal op: must connect directly to Output. Input may be a scalar wire
  // (treated as width-1 vector) or a vector wire produced by VectorCompose /
  // VectorProject.
  TopK,
  // Stateful 1->many
  ResamplerHermite, ResamplerConstant,
  // Sync / routing
  Join, Demux, Mux,
  // N-port sync + linear combination: result = sum_i coeffs[i] * v_i
  Linear,
  // N-port sync + sequential left-fold reduce. Sub-op selected via reduce_op.
  ReduceJoin,
  // N-port sync; the synced N scalars become a width-N vector on output port 0.
  VectorCompose,
  // N-port sync + RPN bytecode evaluator. After syncing N scalar inputs,
  // walks fe_bytecode and emits fe_num_outputs doubles on output port 0.
  // Inherits Join port-queue semantics; output is VectorCompose-shaped.
  FusedExpression,
  // 1-input vector-wire RPN bytecode evaluator. Same opcode set as
  // FusedExpression but INPUT k reads lane k of the upstream vector wire
  // (no per-port queues, no Join sync). Output is a vector of width
  // fe_num_outputs delivered as an SSA [M x double] wire.
  FusedExpressionVector,
  // Burst-oriented aggregator. Reads a vector input, runs an aggregate
  // bytecode (CUMSUM/COUNT/MAX_AGG/etc.) per row, walks an optional
  // segment-predicate bytecode to detect transitions, and emits a vector
  // [key_columns..., agg_outputs...] on each transition. Output is a vector
  // of width (key_columns.size() + num_agg_outputs) — the previous segment's
  // last valid aggregate stamped with the transitioning row's timestamp.
  BurstAggregate,
  // Stateless 1->1 vector ops.
  // VectorExtract: input is a width-N vector wire, output is the scalar at
  // a fixed index.
  // VectorProject: input is a width-N vector wire, output is a width-M vector
  // built from a fixed list of indices.
  VectorExtract, VectorProject,
  // I/O boundary
  Input, Output,
  // Program-level output suppression gate (WHERE predicate)
  Gate,
  // Cross-expression state reader — reads a slot owned by another opcode.
  StateLoad,
  // Composite: Pipeline — wraps an inner sub-program plus optional
  // segment-key bytecode that drives state-reset on key change. The inner
  // program is compiled into its own CompiledGraph and emitted as a
  // sub-function; the outer Pipeline node handles port queues, segment-key
  // tracking, inner state reset, and output buffering.
  Pipeline,
  // Composite: KeyedPipeline — per-key dispatch of an inner sub-program. On
  // every tick, the key is read from a fixed lane (simple-key mode) or
  // computed via polynomial hash over selected lanes (computed-key mode).
  // Each unique key gets its own continuously-running inner state buffer,
  // allocated lazily by a runtime helper. The inner sub-function is shared
  // across keys; only state differs.
  KeyedPipeline,
};

// Reduce variant for OpKind::ReduceJoin. Discriminator selects the
// per-element fold function. Each variant matches an FE ReduceJoin subclass:
//   AddReduce  - Addition       (init=0.0,  combine: a+b)
//   SubReduce  - Subtraction    (init=null, combine: a-b)
//   MulReduce  - Multiplication (init=1.0,  combine: a*b)
//   DivReduce  - Division       (init=null, combine: a/b; no emit on /0)
//   AndReduce  - LogicalAnd     (init=true,  combine: a && b)
//   OrReduce   - LogicalOr      (init=false, combine: a || b)
//   XorReduce  - LogicalXor     (init=null, combine: a != b)
//   NandReduce - LogicalNand    (init=true,  combine: !(a && b))
//   NorReduce  - LogicalNor     (init=true,  combine: !(a || b))
//   XnorReduce - LogicalXnor    (init=null, combine: a == b)
//   ImplReduce - LogicalImplication (init=true, combine: !a || b)
enum class ReduceOp {
  AddReduce, SubReduce, MulReduce, DivReduce,
  AndReduce, OrReduce, XorReduce, NandReduce, NorReduce, XnorReduce, ImplReduce,
};

// One operator instance in the user's pipeline.
struct OpNode {
  std::string id;          // matches the JSON id field
  OpKind kind;
  // Per-kind parameters. Only the fields relevant to `kind` are read.
  std::size_t window_size{0};      // MA, StdDev, PeakDetector, Resampler-buffer (always 4)
  double scale_constant{0.0};      // Scale's constant
  std::int64_t resampler_interval{0};  // ResamplerHermite/Constant dt
  bool         resampler_t0_set{false};       // ResamplerConstant: t0 provided
  std::int64_t resampler_t0{0};               // ResamplerConstant: t0 value
  bool         resampler_snap_first{false};   // ResamplerConstant: snap_first flag
  std::vector<std::string> port_types; // Input/Output port types from JSON
  std::vector<double> coefficients;    // FIR: per-tap weights (size == window_size)
                                       // IIR: b_len b-coeffs followed by a_len a-coeffs
  std::size_t b_len{0};            // IIR: number of b (feedforward) coefficients
  std::size_t a_len{0};            // IIR: number of a (feedback) coefficients
  // StateLoad: id of the op whose state slot to read. Empty for all other ops.
  std::string state_source_id;
  // Constant: emitted output value (input value is ignored).
  double constant_value{0.0};
  // TimeShift: amount added to the input timestamp.
  std::int64_t time_shift{0};
  // Replace family: comparison threshold and substitution value.
  double replace_threshold{0.0};
  double replace_by{0.0};
  // Scalar 1-input ops (AddScalar, PowerScalar, *Scalar comparisons,
  // FiltGt/Lt/Eq/NeqScalar): operand / threshold / equality target.
  double scalar_value{0.0};
  // Tolerance/epsilon for EqTol, NeqTol, EqScalar, NeqScalar,
  // FiltEq/NeqScalar, FiltEq/NeqSync.
  double tolerance{0.0};
  // Function: baked point table (sorted ascending by x), interpolation mode,
  // and pre-computed tangents (only populated when use_hermite is true).
  std::vector<std::pair<double, double>> function_points;
  bool function_use_hermite{false};
  std::vector<double> function_tangents;
  // N-input sync ops (Join, Linear, ReduceJoin): number of input ports.
  // Defaults to 0; parser sets the explicit value or it is inferred from
  // connections / coefficients during emission.
  std::size_t join_num_ports{0};
  // ReduceJoin: which per-element fold to apply.
  ReduceOp reduce_op{ReduceOp::AddReduce};
  // Demultiplexer / Multiplexer: number of (data|control) port pairs.
  // For Demux: 1 data + N control + N output. For Mux: N data + N control + 1 output.
  std::size_t mux_num_ports{0};

  // TopK: capacity (max rows retained), score column index in each row, and
  // sort direction (true=best is largest). row_width is the lane count of one
  // row; resolved from upstream wire width (1 for scalar inputs, N for a
  // vector wire produced by VectorCompose / VectorProject).
  std::size_t topk_k{0};
  std::size_t topk_score_index{0};
  bool        topk_descending{true};
  std::size_t topk_row_width{1};

  // FusedExpression parameters (only populated when kind == FusedExpression).
  // fe_bytecode is the public, unpacked bytecode (interleaved opcode + inline
  // args). fe_constants is referenced by CONST opcodes. fe_coefficients holds
  // FIR/IIR taps. fe_state_init optionally seeds initial state slots; the
  // remainder are filled per FusedStateLayout (zero / -inf / +inf).
  std::size_t fe_num_ports{0};
  std::size_t fe_num_outputs{0};
  std::vector<double> fe_bytecode;
  std::vector<double> fe_constants;
  std::vector<double> fe_coefficients;
  std::vector<double> fe_state_init;

  // BurstAggregate parameters (only populated when kind == BurstAggregate).
  //   ba_agg_bytecode / ba_agg_constants  : per-row aggregate RPN program;
  //                                          must end with num_agg_outputs END
  //                                          markers (one per output column).
  //   ba_seg_bytecode / ba_seg_constants  : optional segment predicate (RPN);
  //                                          empty = never transition.
  //   ba_key_columns                      : input column indices passed
  //                                          through to the output vector.
  //   ba_num_agg_outputs                  : count of END markers in
  //                                          ba_agg_bytecode (output cols
  //                                          appended after the keys).
  //   ba_num_input_cols                   : declared input vector width.
  std::vector<double> ba_agg_bytecode;
  std::vector<double> ba_agg_constants;
  std::vector<double> ba_seg_bytecode;
  std::vector<double> ba_seg_constants;
  std::vector<std::size_t> ba_key_columns;
  std::size_t ba_num_agg_outputs{0};
  std::size_t ba_num_input_cols{0};

  // Pipeline parameters (only populated when kind == Pipeline). The inner
  // program is parsed into its own CompiledGraph; StateLayout fills the
  // inner state size and the baked init pattern. JitCompiler later assigns
  // the inner sub-function name for the IR-level call site.
  std::shared_ptr<CompiledGraph> pipeline_inner_graph;
  std::size_t pipeline_inner_state_size{0};
  std::vector<double> pipeline_inner_state_init;
  std::vector<double> pipeline_segment_bytecode;
  std::vector<double> pipeline_segment_constants;
  std::vector<std::string> pipeline_input_port_types;
  std::vector<std::string> pipeline_output_port_types;
  // pipeline output port idx -> (inner_op_id, inner_op_port). One inner
  // emission may target multiple outer pipeline output ports.
  std::vector<std::pair<std::size_t, std::pair<std::string, std::size_t>>>
      pipeline_output_mappings;
  std::string pipeline_inner_fn_name;
  // Segment-bytecode mode only: lane count of the upstream vector wire
  // feeding Pipeline's data port 0. Resolved post-parse from the upstream
  // node's output_port_width(from_port). The Pipeline state allocates
  // pipeline_input_lane_width port queues (one per lane) so the upstream
  // can fan a width-N vector across N consecutive flat ports.
  std::size_t pipeline_input_lane_width{0};

  // KeyedPipeline parameters (only populated when kind == KeyedPipeline).
  // The inner program is the per-key prototype; one shared sub-function is
  // emitted for it. Unlike Pipeline there is no segment_bytecode and no
  // state-reset on key change — every distinct key gets its own continuously
  // running inner state buffer (allocated lazily by a runtime helper).
  std::shared_ptr<CompiledGraph> keyed_pipeline_inner_graph;
  std::size_t keyed_pipeline_inner_state_size{0};
  std::vector<double> keyed_pipeline_inner_state_init;
  std::string keyed_pipeline_inner_fn_name;
  // Simple-key mode: lane index >= 0. Output is [key, prototype_output...].
  // Computed-key mode: lane index == -1, indices below populated. Output is
  // prototype_output directly (no key prepend).
  int keyed_pipeline_key_index{-1};
  std::vector<int> keyed_pipeline_key_column_indices;
  std::vector<double> keyed_pipeline_key_coefficients;
  std::vector<std::pair<std::size_t, std::pair<std::string, std::size_t>>>
      keyed_pipeline_output_mappings;
  std::vector<std::string> keyed_pipeline_output_port_types;
  std::size_t keyed_pipeline_input_lane_width{0};

  // MovingKeyCount: window size (W). The state layout reserves W ring slots
  // plus ring_count + ring_head + ctx_ptr.
  std::size_t mkc_window_size{0};

  // VectorExtract: index into the upstream vector wire.
  std::size_t vector_index{0};
  // VectorProject: ordered list of indices into the upstream vector wire.
  // The output is a vector whose width equals indices.size().
  std::vector<std::size_t> vector_indices;

  // Per-port scalar width. For most ops this represents output port widths;
  // for the Output op it represents per-collected-port widths (one entry per
  // input port that is exported). An empty vector means "all ports have
  // width 1" (the scalar baseline). Entries past the end of the vector are
  // also treated as width 1, so callers can leave trailing widths unset.
  // Today's JIT opcodes are all width-1 so this is left empty by the parser
  // unless a future vector-typed op populates it.
  std::vector<std::size_t> port_widths;

  // Width of one of this op's output ports. Returns 1 when port_widths is
  // empty/short or the entry is 0 (treat 0 as unset).
  std::size_t output_port_width(std::size_t port) const {
    if (port >= port_widths.size()) return 1;
    const std::size_t w = port_widths[port];
    return w == 0 ? 1 : w;
  }
};

// Distinguishes data ports (i{n}/o{n}) from control ports (c{n}). Existing
// opcodes consume only data inputs; control routing (Demux/Mux) is layered
// on top of this metadata.
enum class PortKind { Data, Control };

struct Connection {
  std::string from_id;
  std::size_t from_port{0};
  std::string to_id;
  std::size_t to_port{0};
  PortKind    from_kind{PortKind::Data};
  PortKind    to_kind{PortKind::Data};
};

// The compiled graph — what JsonParser produces and SegmentPartitioner consumes.
struct CompiledGraph {
  std::vector<OpNode> nodes;
  std::vector<Connection> connections;
  // Output mapping: output op id -> ordered list of port names that are
  // collected. Mirrors the existing Program JSON's "output" field.
  std::map<std::string, std::vector<std::string>> outputs;
  // The entry input op id, mirrors the JSON's "entryOperator" field.
  std::string entry_op_id;
};

}  // namespace rtbot::jit

#endif  // RTBOT_JIT_COMPILED_GRAPH_H
