#include "rtbot/compiled/jit/JitCache.h"

#include <utility>

#include "rtbot/compiled/jit/JitCompiler.h"

namespace rtbot::jit {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::unique_ptr<JitCompiledProgram> JitCache::get_or_compile(
    const std::string& json) {
  std::lock_guard<std::mutex> lock(mu_);

  auto it = entries_.find(json);
  if (it != entries_.end()) {
    return instantiate_from(it->second);
  }

  // Cache miss: compile outside the lock would be preferable for throughput,
  // but for phase 8c correctness wins over parallelism. One compilation at a
  // time per JitCache instance.
  Entry entry = compile_fresh(json);
  auto prog = instantiate_from(entry);
  entries_.emplace(json, std::move(entry));
  return prog;
}

void JitCache::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  entries_.clear();
}

std::size_t JitCache::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return entries_.size();
}

// static
JitCache& JitCache::instance() {
  static JitCache cache;
  return cache;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::unique_ptr<JitCompiledProgram> JitCache::instantiate_from(
    const Entry& entry) {
  auto prog = std::make_unique<JitCompiledProgram>();

  // Fresh, independently zeroed state buffer with overrides applied.
  prog->state_.resize(entry.state_size_doubles, 0.0);
  for (const auto& [off, val] : entry.state_init_overrides) {
    prog->state_[off] = val;
  }
  prog->state_init_overrides_ = entry.state_init_overrides;

  prog->num_outputs_ = entry.num_outputs;
  prog->max_emits_per_call_ = entry.max_emits_per_call;
  prog->segment_fn_  = entry.segment_fn;
  prog->segment_fn_vec_ = entry.segment_fn_vec;
  prog->input_is_vector_ = entry.input_is_vector;
  prog->input_lane_width_ = entry.input_lane_width;

  // Scratch + emit buffers, same sizing convention as JitCompiler.
  prog->out_t_buf_.resize(entry.max_emits_per_call, 0);
  prog->out_v_buf_.resize(entry.max_emits_per_call * entry.num_outputs, 0.0);
  prog->out_port_id_buf_.resize(entry.max_emits_per_call, 0);
  prog->emit_buf_.reserve(500000 * (2 + entry.num_outputs));

  // Share ownership of the compiled code — the entry keeps the primary ref.
  prog->jit_ctx_ = entry.jit_ctx;

  // Re-allocate per-KeyedPipeline-node runtime contexts for this instance and
  // patch their pointers into the fresh state buffer. The compiled IR (shared
  // across instances) reads the pointer from the state slot at runtime, so
  // each instance gets its own per-key state map.
  prog->keyed_pipeline_configs_ = entry.keyed_pipeline_configs;
  for (const auto& cfg : prog->keyed_pipeline_configs_) {
    auto kp_ctx = std::make_unique<KeyedPipelineNodeCtx>();
    kp_ctx->init_pattern = cfg.init_pattern;
    kp_ctx->state_size   = cfg.state_size;
    void* ctx_void = kp_ctx.get();
    double ctx_as_double = 0.0;
    std::memcpy(&ctx_as_double, &ctx_void, sizeof(ctx_void));
    prog->state_[cfg.ctx_ptr_state_slot] = ctx_as_double;
    prog->keyed_pipeline_ctxs_.push_back(std::move(kp_ctx));
  }

  // MovingKeyCount: allocate a fresh per-instance hashmap context and patch
  // its pointer into the state slot recorded in the entry's config.
  prog->moving_key_count_configs_ = entry.moving_key_count_configs;
  for (const auto& cfg : prog->moving_key_count_configs_) {
    auto mkc_ctx = std::make_unique<MovingKeyCountNodeCtx>();
    void* ctx_void = mkc_ctx.get();
    double ctx_as_double = 0.0;
    std::memcpy(&ctx_as_double, &ctx_void, sizeof(ctx_void));
    prog->state_[cfg.ctx_ptr_state_slot] = ctx_as_double;
    prog->moving_key_count_ctxs_.push_back(std::move(mkc_ctx));
  }

  return prog;
}

JitCache::Entry JitCache::compile_fresh(const std::string& json) {
  JitCompiler compiler;
  auto compiled = compiler.compile(json);

  Entry entry;
  entry.segment_fn          = compiled->segment_fn_;
  entry.segment_fn_vec      = compiled->segment_fn_vec_;
  entry.input_is_vector     = compiled->input_is_vector_;
  entry.input_lane_width    = compiled->input_lane_width_;
  entry.state_size_doubles  = compiled->state_.size();
  entry.num_outputs         = compiled->num_outputs_;
  entry.max_emits_per_call  = compiled->max_emits_per_call_;
  entry.state_init_overrides = compiled->state_init_overrides_;
  entry.keyed_pipeline_configs = compiled->keyed_pipeline_configs_;
  entry.moving_key_count_configs = compiled->moving_key_count_configs_;
  // Transfer ownership of the JitContext into the entry's shared_ptr.
  // The compiled program's jit_ctx_ is already a shared_ptr; we move it.
  entry.jit_ctx             = std::move(compiled->jit_ctx_);

  return entry;
}

}  // namespace rtbot::jit
