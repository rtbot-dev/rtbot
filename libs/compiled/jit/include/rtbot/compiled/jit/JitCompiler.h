#ifndef RTBOT_JIT_JIT_COMPILER_H
#define RTBOT_JIT_JIT_COMPILER_H

#include <memory>
#include <string>

#include "rtbot/compiled/jit/JitCompiledProgram.h"

namespace rtbot::jit {

class JitCompiler {
 public:
  // Parse the rtbot Program JSON, partition the graph, plan state layout,
  // emit IR, JIT-compile via ORC, return a runnable program.
  //
  // Throws std::runtime_error on:
  //   - malformed JSON
  //   - unknown opcode
  //   - graph contains sync/routing operators (Join, Demux) — out of scope for phase 7
  //   - JIT compilation failure
  std::unique_ptr<JitCompiledProgram> compile(const std::string& pipeline_json);
};

// One-shot probe: try to compile and call a trivial identity function via ORC JIT.
// Cached — subsequent calls return the cached result without re-probing.
//
// Returns true if ORC JIT works in this environment, false on any non-fatal
// failure (ORC error, allocation failure, exception).
//
// Cannot detect SIGKILL paths (macOS hardened runtime without allow-jit
// entitlement, hardened systemd MemoryDenyWriteExecute). Those are
// deployment-config requirements documented separately.
bool probe_runtime_support();

}  // namespace rtbot::jit

#endif  // RTBOT_JIT_JIT_COMPILER_H
