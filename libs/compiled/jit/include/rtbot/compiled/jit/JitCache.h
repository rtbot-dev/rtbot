#ifndef RTBOT_JIT_JIT_CACHE_H
#define RTBOT_JIT_JIT_CACHE_H

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "rtbot/compiled/jit/JitCompiledProgram.h"

namespace rtbot::jit {

// Process-wide cache of JIT-compiled pipeline artifacts keyed by JSON string.
//
// On a cache hit the compiled function pointer and JitContext are shared;
// each returned JitCompiledProgram gets its own freshly zeroed state buffer
// so instances are fully independent.
//
// Thread-safe: a single mutex guards all map mutations and reads.
class JitCache {
 public:
  JitCache() = default;

  // Get-or-compile. On cache miss, compiles via JitCompiler and stores the
  // artifact. On cache hit, returns a fresh JitCompiledProgram with a new
  // zero-initialized state buffer reusing the cached function pointer.
  //
  // Throws (via JitCompiler::compile) on compile failure — caller is
  // responsible for catching and falling back.
  std::unique_ptr<JitCompiledProgram> get_or_compile(const std::string& json);

  // Clear all cached entries (for testing).
  void clear();

  // Number of entries currently in the cache.
  std::size_t size() const;

  // Process-wide singleton.
  static JitCache& instance();

 private:
  struct Entry {
    JitCompiledProgram::SegmentFnT segment_fn{nullptr};
    JitCompiledProgram::SegmentFnVecT segment_fn_vec{nullptr};
    bool input_is_vector{false};
    std::size_t input_lane_width{1};
    std::size_t state_size_doubles{0};
    std::size_t num_outputs{0};
    std::size_t max_emits_per_call{1};
    std::map<std::size_t, double> state_init_overrides;
    std::shared_ptr<rtbot::JitContext> jit_ctx;  // keeps compiled code alive
    // Per-KeyedPipeline-node runtime config. Each instantiation allocates
    // fresh KeyedPipelineNodeCtx instances and patches their pointers into
    // the per-instance state buffer.
    std::vector<KeyedPipelineNodeConfig> keyed_pipeline_configs;
    // Per-MovingKeyCount-node runtime config; one ctx per instance.
    std::vector<MovingKeyCountNodeConfig> moving_key_count_configs;
  };

  // Build a fresh JitCompiledProgram from a cached Entry.
  std::unique_ptr<JitCompiledProgram> instantiate_from(const Entry& entry);

  // Compile a fresh entry from JSON via JitCompiler.
  Entry compile_fresh(const std::string& json);

  mutable std::mutex mu_;
  // Keyed by full JSON string — no collision risk, lookup cost negligible for
  // typical pipeline JSON sizes.
  std::map<std::string, Entry> entries_;
};

}  // namespace rtbot::jit

#endif  // RTBOT_JIT_JIT_CACHE_H
