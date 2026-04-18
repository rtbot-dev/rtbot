#ifndef RTBOT_PERF_COUNTERS_H
#define RTBOT_PERF_COUNTERS_H

// Lightweight per-phase timing aggregator for hot-path measurement.
//
// Enabled with -DRTBOT_PERF=1 at compile time. When disabled (default), all
// macros expand to nothing and there is zero runtime cost.
//
// Usage:
//   RTBOT_PERF_SCOPE(EMIT_CLONE);          // RAII timer covering current scope
//   RTBOT_PERF_COUNT(MSG_ALLOC_POOL_HIT);  // just bump the counter, no timing
//
// Report (end of benchmark run):
//   rtbot::PerfCounters::dump(std::cout);
//   rtbot::PerfCounters::reset();

#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <string>

namespace rtbot {

enum class PerfPhase : int {
  EMIT_CLONE = 0,
  EMIT_DISPATCH,
  RECEIVE_DATA,
  PROCESS_DATA,
  QUEUE_PUSH,
  QUEUE_POP,
  MSG_ALLOC_POOL_HIT,
  MSG_ALLOC_POOL_MISS,
  _COUNT
};

inline const char* perf_phase_name(PerfPhase p) {
  switch (p) {
    case PerfPhase::EMIT_CLONE: return "EMIT_CLONE";
    case PerfPhase::EMIT_DISPATCH: return "EMIT_DISPATCH";
    case PerfPhase::RECEIVE_DATA: return "RECEIVE_DATA";
    case PerfPhase::PROCESS_DATA: return "PROCESS_DATA";
    case PerfPhase::QUEUE_PUSH: return "QUEUE_PUSH";
    case PerfPhase::QUEUE_POP: return "QUEUE_POP";
    case PerfPhase::MSG_ALLOC_POOL_HIT: return "MSG_ALLOC_POOL_HIT";
    case PerfPhase::MSG_ALLOC_POOL_MISS: return "MSG_ALLOC_POOL_MISS";
    default: return "?";
  }
}

struct PerfCell {
  uint64_t calls{0};
  uint64_t ns{0};
};

class PerfCounters {
 public:
  static constexpr int N = static_cast<int>(PerfPhase::_COUNT);

  // Flat array per thread. First call lazily allocates.
  static std::array<PerfCell, N>& tls() {
    thread_local std::array<PerfCell, N> cells{};
    return cells;
  }

  static void add(PerfPhase p, uint64_t ns) {
    auto& c = tls()[static_cast<int>(p)];
    c.calls++;
    c.ns += ns;
  }

  static void count(PerfPhase p) {
    tls()[static_cast<int>(p)].calls++;
  }

  static void reset() {
    auto& cells = tls();
    for (auto& c : cells) c = {};
  }

  static void dump(std::ostream& os) {
    const auto& cells = tls();
    uint64_t total_ns = 0;
    for (const auto& c : cells) total_ns += c.ns;
    if (total_ns == 0) total_ns = 1;  // avoid div-by-zero

    os << "\nPerfCounters (per-phase, this thread):\n";
    os << std::left << std::setw(22) << "phase"
       << std::right << std::setw(14) << "calls"
       << std::setw(16) << "total_ns"
       << std::setw(12) << "ns/call"
       << std::setw(10) << "%" << "\n";
    os << std::string(74, '-') << "\n";
    for (int i = 0; i < N; ++i) {
      const auto& c = cells[i];
      double nspc = c.calls ? static_cast<double>(c.ns) / c.calls : 0.0;
      double pct = 100.0 * static_cast<double>(c.ns) / static_cast<double>(total_ns);
      os << std::left << std::setw(22) << perf_phase_name(static_cast<PerfPhase>(i))
         << std::right << std::setw(14) << c.calls
         << std::setw(16) << c.ns
         << std::setw(12) << std::fixed << std::setprecision(1) << nspc
         << std::setw(9) << std::fixed << std::setprecision(1) << pct << "%"
         << "\n";
    }
    os << std::string(74, '-') << "\n";
    os << std::left << std::setw(22) << "TOTAL"
       << std::right << std::setw(14) << ""
       << std::setw(16) << total_ns
       << "\n";
  }
};

#ifdef RTBOT_PERF

class PerfScope {
 public:
  explicit PerfScope(PerfPhase p) : phase_(p), start_(std::chrono::steady_clock::now()) {}
  ~PerfScope() {
    auto end = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
    PerfCounters::add(phase_, static_cast<uint64_t>(ns));
  }
 private:
  PerfPhase phase_;
  std::chrono::steady_clock::time_point start_;
};

#define RTBOT_PERF_SCOPE(phase) ::rtbot::PerfScope rtbot_perf_scope_##__LINE__{::rtbot::PerfPhase::phase}
#define RTBOT_PERF_COUNT(phase) ::rtbot::PerfCounters::count(::rtbot::PerfPhase::phase)

#else

#define RTBOT_PERF_SCOPE(phase) do {} while (0)
#define RTBOT_PERF_COUNT(phase) do {} while (0)

#endif

}  // namespace rtbot

#endif  // RTBOT_PERF_COUNTERS_H
