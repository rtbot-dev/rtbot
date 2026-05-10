#ifndef RTBOT_COMPILED_JOIN_STAGE_H
#define RTBOT_COMPILED_JOIN_STAGE_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace rtbot::compiled {

// Per-port capacity for a JoinStage. If a port queue exceeds this, the
// oldest entry is dropped — the same semantics rtbot::Join enforces via
// MAX_SIZE_PER_PORT, just at a smaller bound for the templated context.
inline constexpr std::size_t kJoinPortCapacity = 64;

namespace detail {

template <std::size_t Cap>
struct PortBuffer {
  std::array<std::int64_t, Cap> times{};
  std::array<double, Cap> values{};
  std::size_t head{0};
  std::size_t size{0};

  inline void push(std::int64_t t, double v) noexcept {
    if (size == Cap) {
      // Drop oldest.
      head = (head + 1) % Cap;
      --size;
    }
    const std::size_t idx = (head + size) % Cap;
    times[idx] = t;
    values[idx] = v;
    ++size;
  }

  inline std::int64_t front_time() const noexcept { return times[head]; }
  inline double front_value() const noexcept { return values[head]; }

  inline void pop_front() noexcept {
    if (size == 0) return;
    head = (head + 1) % Cap;
    --size;
  }
};

}  // namespace detail

// N-port timestamp synchronizer. Each port has its own bounded ring buffer.
// Push to a port at (t, v); if every port now has a front entry at the same
// timestamp, returns true with that timestamp + the N values, and pops one
// entry from each port. Older mismatched fronts are discarded.
template <std::size_t N>
struct JoinStage {
  static_assert(N >= 2, "JoinStage requires N >= 2");

  std::array<detail::PortBuffer<kJoinPortCapacity>, N> ports{};

  inline bool push(std::size_t port, std::int64_t t, double v,
                   std::int64_t& out_t,
                   std::array<double, N>& out_vs) noexcept {
    ports[port].push(t, v);
    return try_sync(out_t, out_vs);
  }

 private:
  inline bool try_sync(std::int64_t& out_t,
                       std::array<double, N>& out_vs) noexcept {
    while (true) {
      // All ports must have at least one entry.
      for (auto& p : ports) {
        if (p.size == 0) return false;
      }
      // Find min and max front times.
      std::int64_t min_t = ports[0].front_time();
      std::int64_t max_t = min_t;
      for (std::size_t i = 1; i < N; ++i) {
        const std::int64_t ft = ports[i].front_time();
        if (ft < min_t) min_t = ft;
        if (ft > max_t) max_t = ft;
      }
      if (min_t == max_t) {
        // Synced — collect values, pop one from each port.
        out_t = min_t;
        for (std::size_t i = 0; i < N; ++i) {
          out_vs[i] = ports[i].front_value();
          ports[i].pop_front();
        }
        return true;
      }
      // Drop fronts equal to min_t on all ports that have it.
      for (auto& p : ports) {
        if (p.size > 0 && p.front_time() == min_t) p.pop_front();
      }
    }
  }
};

}  // namespace rtbot::compiled

#endif  // RTBOT_COMPILED_JOIN_STAGE_H
