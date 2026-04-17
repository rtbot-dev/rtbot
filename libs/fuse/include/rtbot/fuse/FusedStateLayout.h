#ifndef RTBOT_FUSE_STATE_LAYOUT_H
#define RTBOT_FUSE_STATE_LAYOUT_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"

namespace rtbot::fuse {

// State footprint for each opcode — the number of double slots it reserves
// starting at its configured state offset. Opcodes that do not mutate state
// are absent from the lookup.
//
// Derivation (Phase 3 plan): ring-buffered opcodes take `win_size + K`
// slots where K depends on the algorithm's auxiliary scalars.
//   MA/MSUM: ring(win) + kahan_sum + kahan_comp + count           (win + 3)
//   STD:     ring(win) + mean + m2 + count + _pad                 (win + 4)
//   WIN_MIN/MAX: ring(win) + deque_indices(win) + deque_head_tail (2*win + 2)
//   FIR:     ring(win) + head + count                             (win + 2)
//   IIR:     (max(num_len, den_len) * 2) + 4                      (explicit)
// Non-windowed stateful opcodes occupy fixed slots:
//   CUMSUM: kahan_sum + kahan_comp                                (2)
//   COUNT:  running count                                         (1)
//   MAX_AGG/MIN_AGG: single accumulator                           (1)
//   STATE_LOAD: borrows another opcode's slot; does not reserve.
//   DIFF/SIGN_CHANGE: previous value + has-previous flag          (2)

struct StateLayout {
  std::size_t total_state_size = 0;
  // Per-slot initial values. Defaults to 0.0; +inf/-inf are written for
  // MIN_AGG/MAX_AGG seed slots.
  std::vector<double> initial_values;
};

namespace detail {

inline void reserve_slots(StateLayout& out, std::size_t off, std::size_t count,
                           double fill) {
  const std::size_t end = off + count;
  if (out.initial_values.size() < end) out.initial_values.resize(end, 0.0);
  for (std::size_t k = off; k < end; ++k) out.initial_values[k] = fill;
  out.total_state_size = std::max(out.total_state_size, end);
}

}  // namespace detail

// Walk packed bytecode + aux_args and compute total state size and per-slot
// initial values. Safe to call once at operator construction — the result
// feeds state_init_.
inline StateLayout compute_state_layout(
    const std::vector<Instruction>& packed,
    const std::vector<AuxArgs>& aux) {
  using namespace rtbot::fused_op;
  StateLayout out{};

  const double kPosInf = std::numeric_limits<double>::infinity();
  const double kNegInf = -std::numeric_limits<double>::infinity();

  for (const Instruction& ins : packed) {
    const std::uint8_t op = ins.op;

    // Non-windowed stateful opcodes: inline arg is the state offset directly.
    if (op == static_cast<std::uint8_t>(CUMSUM)) {
      detail::reserve_slots(out, ins.arg, 2, 0.0);
    } else if (op == static_cast<std::uint8_t>(COUNT)) {
      detail::reserve_slots(out, ins.arg, 1, 0.0);
    } else if (op == static_cast<std::uint8_t>(MAX_AGG)) {
      detail::reserve_slots(out, ins.arg, 1, kNegInf);
    } else if (op == static_cast<std::uint8_t>(MIN_AGG)) {
      detail::reserve_slots(out, ins.arg, 1, kPosInf);
    } else if (op == static_cast<std::uint8_t>(DIFF) ||
               op == static_cast<std::uint8_t>(SIGN_CHANGE)) {
      detail::reserve_slots(out, ins.arg, 2, 0.0);
    }

    // Windowed opcodes: inline arg indexes into aux_args.
    // Aux layout: {state_off, win_size, coeff_off (FIR/IIR), coeff_len}.
    else if (op == static_cast<std::uint8_t>(MA_UPDATE) ||
             op == static_cast<std::uint8_t>(MSUM_UPDATE)) {
      const AuxArgs& a = aux[ins.arg];
      detail::reserve_slots(out, a.a, std::size_t{a.b} + 3, 0.0);
    } else if (op == static_cast<std::uint8_t>(STD_UPDATE)) {
      const AuxArgs& a = aux[ins.arg];
      detail::reserve_slots(out, a.a, std::size_t{a.b} + 4, 0.0);
    } else if (op == static_cast<std::uint8_t>(WIN_MIN)) {
      const AuxArgs& a = aux[ins.arg];
      detail::reserve_slots(out, a.a, 2 * std::size_t{a.b} + 2, kPosInf);
    } else if (op == static_cast<std::uint8_t>(WIN_MAX)) {
      const AuxArgs& a = aux[ins.arg];
      detail::reserve_slots(out, a.a, 2 * std::size_t{a.b} + 2, kNegInf);
    } else if (op == static_cast<std::uint8_t>(FIR_UPDATE)) {
      const AuxArgs& a = aux[ins.arg];
      detail::reserve_slots(out, a.a, std::size_t{a.b} + 2, 0.0);
    } else if (op == static_cast<std::uint8_t>(IIR_UPDATE)) {
      const AuxArgs& a = aux[ins.arg];
      const std::size_t max_len = std::max(std::size_t{a.b}, std::size_t{a.d});
      detail::reserve_slots(out, a.a, max_len * 2 + 4, 0.0);
    }
    // STATE_LOAD borrows another opcode's slot — no reservation here.
  }

  return out;
}

}  // namespace rtbot::fuse

#endif  // RTBOT_FUSE_STATE_LAYOUT_H
