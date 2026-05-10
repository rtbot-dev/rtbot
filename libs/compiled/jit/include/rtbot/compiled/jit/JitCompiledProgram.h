#ifndef RTBOT_JIT_JIT_COMPILED_PROGRAM_H
#define RTBOT_JIT_JIT_COMPILED_PROGRAM_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "libs/compiled/jit_spike/JitContext.h"

namespace rtbot::jit {

// Per-KeyedPipeline-node runtime context. Holds the lazy-allocated per-key
// state buffers plus the init pattern + state size needed to spawn a fresh
// buffer on first sight of a key. Lifetime: owned by JitCompiledProgram.
struct KeyedPipelineNodeCtx {
  std::unordered_map<double, std::vector<double>> per_key_state;
  std::vector<double> init_pattern;
  std::size_t state_size{0};
};

// Per-MovingKeyCount-node runtime context. Holds the per-instance hashmap
// of per-key counts. Lifetime: owned by JitCompiledProgram. The IR-side ring
// buffer + accounting lives in the JIT's static state buffer; only the
// hashmap is dynamic.
struct MovingKeyCountNodeCtx {
  std::unordered_map<double, std::size_t> counts;
};

// Configuration captured at JIT compile time for one MovingKeyCount node.
// JitCompiledProgram (and JitCache) use this to spawn fresh contexts per
// program and to patch their pointers into the per-instance state buffer.
struct MovingKeyCountNodeConfig {
  std::size_t ctx_ptr_state_slot{0};
};

// Configuration captured at JIT compile time for one KeyedPipeline node.
// Used by JitCompiledProgram (and JitCache) to spawn fresh KeyedPipelineNodeCtx
// instances per program and to patch their pointers into the per-instance
// state buffer at the slot the IR reads.
struct KeyedPipelineNodeConfig {
  std::size_t ctx_ptr_state_slot{0};
  std::vector<double> init_pattern;
  std::size_t state_size{0};
};

// One emitted record from the JIT'd pipeline.
// timestamp + variable-length output values (size = num_outputs).
// port_id identifies which output port produced this emission. For single-port
// pipelines (the only kind today) it is always 0; reserved for Demux/Mux.
struct EmittedRecord {
  std::int64_t time;
  std::int32_t port_id;
  std::vector<double> values;
};

// Drop-in equivalent of CompiledProgram for the JIT path. Created by
// JitCompiledProgram::compile(); holds the JIT'd code alive via JitContext.
//
// Multi-emit ABI: SegmentFnT writes up to `max_emits_per_call_` (K) records
// into the caller-provided arrays and returns the count. Existing single-emit
// programs use K=1 and return 0 or 1. Demultiplexer programs use K =
// max num_ports and may return > 1 when multiple control ports fire.
class JitCompiledProgram {
 public:
  using SegmentFnT = std::int32_t (*)(double* state, std::int64_t t, double v,
                                       std::int64_t* out_t_arr,
                                       double* out_v_arr,
                                       std::int32_t* out_port_id_arr);

  // Vector-input variant: when the program was compiled with input lane width
  // N > 1, the emitted segment function takes a `const double* in_v_arr`
  // pointer to N consecutive lane values instead of a scalar `double v`.
  using SegmentFnVecT = std::int32_t (*)(double* state, std::int64_t t,
                                          const double* in_v_arr,
                                          std::int64_t* out_t_arr,
                                          double* out_v_arr,
                                          std::int32_t* out_port_id_arr);

  // Drive one input through the JIT'd pipeline. May or may not emit. When K>1
  // a single call may produce multiple records (Demultiplexer fan-out).
  // Always-inlined so the compiler can hoist state_.data() / segment_fn_ /
  // out_v_buf_ out of a tight caller loop and eliminate call overhead.
  __attribute__((always_inline)) void receive(std::int64_t t, double v) {
    std::int32_t count = segment_fn_(state_.data(), t, v,
                                      out_t_buf_.data(),
                                      out_v_buf_.data(),
                                      out_port_id_buf_.data());
    for (std::int32_t r = 0; r < count; ++r) {
      double t_bits;
      std::int64_t rec_t = out_t_buf_[r];
      std::memcpy(&t_bits, &rec_t, sizeof(double));
      emit_buf_.push_back(t_bits);
      double pid_bits = 0.0;
      std::int32_t rec_pid = out_port_id_buf_[r];
      std::memcpy(&pid_bits, &rec_pid, sizeof(std::int32_t));
      emit_buf_.push_back(pid_bits);
      const double* row = out_v_buf_.data() + static_cast<std::size_t>(r) * num_outputs_;
      for (std::size_t i = 0; i < num_outputs_; ++i) {
        emit_buf_.push_back(row[i]);
      }
    }
  }

  // Vector-input variant of receive(). Pre-condition: this program was
  // compiled with input lane width == lane_width (matches input_lane_width()).
  // The emitted IR reads N consecutive doubles from `values`; callers must
  // provide at least lane_width elements.
  __attribute__((always_inline)) void receive_vector(std::int64_t t,
                                                      const double* values,
                                                      std::size_t /*lane_width*/) {
    std::int32_t count = segment_fn_vec_(state_.data(), t, values,
                                          out_t_buf_.data(),
                                          out_v_buf_.data(),
                                          out_port_id_buf_.data());
    for (std::int32_t r = 0; r < count; ++r) {
      double t_bits;
      std::int64_t rec_t = out_t_buf_[r];
      std::memcpy(&t_bits, &rec_t, sizeof(double));
      emit_buf_.push_back(t_bits);
      double pid_bits = 0.0;
      std::int32_t rec_pid = out_port_id_buf_[r];
      std::memcpy(&pid_bits, &rec_pid, sizeof(std::int32_t));
      emit_buf_.push_back(pid_bits);
      const double* row = out_v_buf_.data() + static_cast<std::size_t>(r) * num_outputs_;
      for (std::size_t i = 0; i < num_outputs_; ++i) {
        emit_buf_.push_back(row[i]);
      }
    }
  }

  // Drain accumulated outputs since last call.
  // Constructs EmittedRecord objects from the flat emit_buf_ on demand.
  std::vector<EmittedRecord> collect_outputs();

  // Zero-allocation walk over accumulated records. For each invokes
  // cb(int64_t time, int32_t port_id, const double* values, size_t n_values).
  // values points into the internal emit_buf_ — do NOT retain past callback
  // return. When `consume == true` (default), emit_buf_ is cleared on exit;
  // pass false for a non-destructive peek (useful for overflow-retry paths).
  template <class Callback>
  __attribute__((always_inline)) void drain_records_raw(Callback&& cb,
                                                          bool consume = true) {
    const std::size_t stride = 2 + num_outputs_;
    if (emit_buf_.empty() || stride == 0) {
      if (consume) emit_buf_.clear();
      return;
    }
    const std::size_t count = emit_buf_.size() / stride;
    for (std::size_t i = 0; i < count; ++i) {
      const double* slot = emit_buf_.data() + i * stride;
      std::int64_t rec_t;
      std::memcpy(&rec_t, slot, sizeof(std::int64_t));
      std::int32_t rec_pid;
      std::memcpy(&rec_pid, slot + 1, sizeof(std::int32_t));
      cb(rec_t, rec_pid, slot + 2, num_outputs_);
    }
    if (consume) emit_buf_.clear();
  }

  // Discard accumulated outputs (clears emit_buf_) without invoking any
  // callback. Used by binding-layer fast paths after a non-destructive
  // peek/serialize completes successfully.
  void discard_emitted() { emit_buf_.clear(); }

  // Read-only accessors used by JitCompiler at construction.
  std::size_t num_outputs() const { return num_outputs_; }
  std::size_t state_size() const { return state_.size(); }
  std::size_t max_emits_per_call() const { return max_emits_per_call_; }
  // True when the program was compiled with a vector input (lane width > 1).
  // In that case the emitted function uses the SegmentFnVecT signature and
  // callers must drive it via receive_vector / segment_fn_vec_.
  bool input_is_vector() const { return input_is_vector_; }
  std::size_t input_lane_width() const { return input_lane_width_; }
  // Expose the raw function pointer for direct-call benchmarking.
  SegmentFnT raw_fn() const { return segment_fn_; }
  SegmentFnVecT raw_fn_vec() const { return segment_fn_vec_; }
  // Expose the raw state buffer for direct-call benchmarking.
  double* raw_state() { return state_.data(); }

  // Hot-path accessors for Program::send's cached-pointer fast path.
  // Program caches these at construction and calls segment_fn_ directly,
  // bypassing the unique_ptr dereference on every tick.
  SegmentFnT raw_segment_fn() const { return segment_fn_; }
  double* raw_state_buffer() { return state_.data(); }
  std::int64_t* raw_out_t_buf() { return out_t_buf_.data(); }
  double* raw_out_v_buf() { return out_v_buf_.data(); }
  std::int32_t* raw_out_port_id_buf() { return out_port_id_buf_.data(); }

  // Record `count` emissions into the flat emit_buf_. Called by Program::send
  // after a direct segment_fn_ invocation returns count > 0.
  // out_t_arr:       length count, raw int64 timestamps
  // out_port_id_arr: length count, int32 output port ids
  // out_v_arr:       row-major, length count * n
  // n:               num_outputs_
  inline void push_emissions(std::int32_t count,
                              const std::int64_t* out_t_arr,
                              const std::int32_t* out_port_id_arr,
                              const double* out_v_arr, std::size_t n) {
    for (std::int32_t r = 0; r < count; ++r) {
      double t_bits;
      std::int64_t rec_t = out_t_arr[r];
      std::memcpy(&t_bits, &rec_t, sizeof(double));
      emit_buf_.push_back(t_bits);
      double pid_bits = 0.0;
      std::int32_t rec_pid = out_port_id_arr[r];
      std::memcpy(&pid_bits, &rec_pid, sizeof(std::int32_t));
      emit_buf_.push_back(pid_bits);
      const double* row = out_v_arr + static_cast<std::size_t>(r) * n;
      for (std::size_t i = 0; i < n; ++i) {
        emit_buf_.push_back(row[i]);
      }
    }
  }

 private:
  friend class JitCompiler;
  friend class JitCache;

  std::vector<double> state_;
  std::size_t num_outputs_{0};
  // Maximum number of emissions per segment_fn call. K = 1 for everything
  // except programs containing Demultiplexer (K = max num_ports across the
  // graph). Drives the size of out_t_buf_ / out_v_buf_ / out_port_id_buf_.
  std::size_t max_emits_per_call_{1};
  // Scalar-input function pointer (input_is_vector_ == false). Holds the same
  // address as segment_fn_vec_ when input_is_vector_ == true; the two are
  // never both used for the same program.
  SegmentFnT segment_fn_{nullptr};
  SegmentFnVecT segment_fn_vec_{nullptr};
  // Width of the program's Input op port. 1 for scalar-input programs;
  // > 1 for vector-input programs (the FE pushes a width-N VectorNumberData
  // per tick).
  std::size_t input_lane_width_{1};
  bool input_is_vector_{false};
  // Non-zero initial values for specific state slots (used by JitCache).
  std::map<std::size_t, double> state_init_overrides_;

  // Flat emit buffer: stores (i64_as_double, i32_as_double_lane, v0, v1, ..., v_{N-1}) per emit.
  // One "record" occupies (2 + num_outputs_) slots. No per-emit allocation.
  // Drained and converted to EmittedRecord by collect_outputs().
  std::vector<double> emit_buf_;

  // Per-call scratch buffers for the JIT'd function's outputs. Sized for
  // K records: out_t_buf_[K], out_v_buf_[K * num_outputs_], out_port_id_buf_[K].
  // Pre-allocated at construction to avoid per-receive heap allocation.
  std::vector<std::int64_t> out_t_buf_;
  std::vector<double>       out_v_buf_;
  std::vector<std::int32_t> out_port_id_buf_;
  // Holds the JitContext alive so the JIT'd code remains valid.
  // shared_ptr so JitCache can share the same compiled artifact across instances.
  std::shared_ptr<rtbot::JitContext> jit_ctx_;

  // Per-instance KeyedPipeline runtime contexts. Each entry owns the lazy
  // per-key state map for one KeyedPipeline node in the graph. Pointers to
  // these are patched into the per-instance state buffer at the slots
  // recorded in keyed_pipeline_configs_ (set up by JitCompiler / JitCache).
  std::vector<std::unique_ptr<KeyedPipelineNodeCtx>> keyed_pipeline_ctxs_;
  // Per-KeyedPipeline-node configuration captured at compile time. Used by
  // JitCache to re-allocate fresh ctxs when re-instantiating from the cache.
  std::vector<KeyedPipelineNodeConfig> keyed_pipeline_configs_;

  // Per-instance MovingKeyCount runtime contexts. Each entry owns the
  // hashmap of per-key counts for one MovingKeyCount node in the graph.
  // Pointers to these are patched into the per-instance state buffer at the
  // slots recorded in moving_key_count_configs_.
  std::vector<std::unique_ptr<MovingKeyCountNodeCtx>> moving_key_count_ctxs_;
  std::vector<MovingKeyCountNodeConfig> moving_key_count_configs_;
};

// Runtime helper exposed to JIT IR. Returns a pointer to the per-key state
// buffer for the given key, lazy-allocating it (and zero-initializing with
// the ctx's init pattern) on first sight. Stable signature so the IR can
// bake a constant function pointer.
extern "C" double* rtbot_jit_keyed_pipeline_lookup(void* ctx_ptr, double key);

// Runtime helper for MovingKeyCount: given the per-instance hashmap ctx,
// the new key being inserted, an evicted key, and a flag indicating whether
// the eviction is valid, returns the count of new_key in the current window
// after applying the increment + (conditional) decrement. The IR-side ring
// buffer is responsible for tracking which key (if any) is leaving the
// window; this helper just updates the hashmap and returns the new count.
extern "C" double rtbot_jit_mkc_step(void* ctx_ptr, double new_key,
                                     double evicted_key, double evict_valid);

}  // namespace rtbot::jit

#endif  // RTBOT_JIT_JIT_COMPILED_PROGRAM_H
