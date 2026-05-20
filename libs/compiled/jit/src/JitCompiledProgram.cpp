#include "rtbot/compiled/jit/JitCompiledProgram.h"

extern "C" double* rtbot_jit_keyed_pipeline_lookup(void* ctx_ptr, double key) {
  auto* ctx = static_cast<rtbot::jit::KeyedPipelineNodeCtx*>(ctx_ptr);
  auto& vec = ctx->per_key_state[key];
  if (vec.empty() && ctx->state_size > 0) {
    vec.assign(ctx->init_pattern.begin(), ctx->init_pattern.end());
  }
  return vec.data();
}

extern "C" double rtbot_jit_mkc_step(void* ctx_ptr, double new_key,
                                     double evicted_key, double evict_valid) {
  auto* ctx = static_cast<rtbot::jit::MovingKeyCountNodeCtx*>(ctx_ptr);
  auto& counts = ctx->counts;
  if (evict_valid != 0.0) {
    auto it = counts.find(evicted_key);
    if (it != counts.end()) {
      if (it->second <= 1) {
        counts.erase(it);
      } else {
        --it->second;
      }
    }
  }
  std::size_t& cnt = counts[new_key];
  ++cnt;
  return static_cast<double>(cnt);
}

namespace rtbot::jit {

std::vector<EmittedRecord> JitCompiledProgram::collect_outputs() {
  const std::size_t stride = 2 + num_outputs_;
  const std::size_t count  = emit_buf_.size() / stride;
  std::vector<EmittedRecord> result;
  result.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const double* slot = emit_buf_.data() + i * stride;
    std::int64_t rec_t;
    std::memcpy(&rec_t, slot, sizeof(std::int64_t));
    std::int32_t rec_pid;
    std::memcpy(&rec_pid, slot + 1, sizeof(std::int32_t));
    std::vector<double> vals(slot + 2, slot + 2 + num_outputs_);
    result.push_back({rec_t, rec_pid, std::move(vals)});
  }
  emit_buf_.clear();
  return result;
}

}  // namespace rtbot::jit
