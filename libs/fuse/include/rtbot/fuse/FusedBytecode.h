#ifndef RTBOT_FUSE_BYTECODE_H
#define RTBOT_FUSE_BYTECODE_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedOps.h"

namespace rtbot::fuse {

// Packed bytecode record for FusedExpression programs.
//
// Replaces the earlier `std::vector<double>` encoding (16 bytes per opcode+arg)
// with a 4-byte record. Opcodes fit in uint8_t (max 43). `arg` is the inline
// argument consumed by INPUT/CONST/stateful opcodes; for windowed opcodes it
// is an index into an internal AuxArgs side table. `flags` is reserved for
// future phases (debug marks, emission-semantics markers).
struct Instruction {
  std::uint8_t op;
  std::uint8_t flags;
  std::uint16_t arg;
};

static_assert(sizeof(Instruction) == 4, "Instruction must be exactly 4 bytes");
static_assert(std::is_trivially_copyable_v<Instruction>,
              "Instruction must be trivially copyable");
static_assert(std::is_standard_layout_v<Instruction>,
              "Instruction must be standard layout");

// Number of inline doubles the given opcode consumes in the caller-facing
// bytecode form. Windowed opcodes (MA/MSUM/STD/WIN_MIN/WIN_MAX) take their
// window size inline; FIR_UPDATE takes (coeff_start, coeff_len); IIR_UPDATE
// takes (b_len, a_len, coeff_start). State offsets for windowed opcodes are
// auto-allocated at pack time and never appear in the public bytecode.
inline std::size_t inline_arg_count(std::uint8_t op) {
  switch (op) {
    case 0  /* INPUT */:
    case 1  /* CONST */:
    case 21 /* CUMSUM */:
    case 22 /* COUNT */:
    case 23 /* MAX_AGG */:
    case 24 /* MIN_AGG */:
    case 25 /* STATE_LOAD */:
    case 35 /* MA_UPDATE */:
    case 36 /* MSUM_UPDATE */:
    case 37 /* STD_UPDATE */:
    case 38 /* DIFF */:
    case 39 /* SIGN_CHANGE */:
    case 40 /* WIN_MIN */:
    case 41 /* WIN_MAX */:
      return 1;
    case 42 /* FIR_UPDATE */:
      return 2;
    case 43 /* IIR_UPDATE */:
      return 3;
    default:
      return 0;
  }
}

// Output of pack_bytecode. `packed` is the 4-byte-per-opcode instruction
// stream. `aux_args` is an internal side table generated from the windowed
// opcodes' inline args (state offset, window size, coefficient pointers);
// callers never see or construct it. `state_init` is the initial state
// vector sized to hold all accumulators and ring buffers used by the program.
struct PackResult {
  std::vector<Instruction> packed;
  std::vector<AuxArgs> aux_args;
  std::vector<double> state_init;
};

// Convert a caller-facing bytecode stream (opcodes interleaved with their
// inline double args) into the packed Instruction form plus the internal
// aux_args side table and a pre-seeded state_init vector.
//
// Auto-allocation: windowed opcodes (MA_UPDATE..IIR_UPDATE) receive state
// offsets placed after any state slots manually claimed by non-windowed
// stateful opcodes (CUMSUM, COUNT, MAX_AGG, MIN_AGG, DIFF, SIGN_CHANGE).
// This keeps manual and auto-allocated regions disjoint so STATE_LOAD can
// still reference a specific accumulator by its manual offset.
inline PackResult pack_bytecode(const std::vector<double>& bc) {
  PackResult out;
  const double kPosInf = std::numeric_limits<double>::infinity();
  const double kNegInf = -std::numeric_limits<double>::infinity();

  auto reserve_state = [&](std::size_t off, std::size_t count, double fill) {
    const std::size_t end = off + count;
    if (out.state_init.size() < end) out.state_init.resize(end, 0.0);
    for (std::size_t k = off; k < end; ++k) out.state_init[k] = fill;
  };

  // First pass: find the max state slot claimed by manually-placed opcodes
  // (CUMSUM/COUNT/MAX_AGG/MIN_AGG/DIFF/SIGN_CHANGE) so windowed opcodes can
  // auto-allocate above that ceiling without colliding.
  std::size_t manual_max_end = 0;
  {
    std::size_t pc = 0;
    while (pc < bc.size()) {
      const auto op = static_cast<std::uint8_t>(bc[pc++]);
      const std::size_t n = inline_arg_count(op);
      if (n >= 1) {
        const std::size_t arg0 = static_cast<std::size_t>(bc[pc]);
        std::size_t end = 0;
        switch (op) {
          case 21 /* CUMSUM */:
          case 38 /* DIFF */:
          case 39 /* SIGN_CHANGE */:
            end = arg0 + 2;
            break;
          case 22 /* COUNT */:
          case 23 /* MAX_AGG */:
          case 24 /* MIN_AGG */:
          case 25 /* STATE_LOAD */:
            end = arg0 + 1;
            break;
          default:
            break;
        }
        if (end > manual_max_end) manual_max_end = end;
      }
      pc += n;
    }
  }

  std::size_t state_cursor = manual_max_end;
  std::size_t pc = 0;
  while (pc < bc.size()) {
    const auto op = static_cast<std::uint8_t>(bc[pc++]);
    const std::size_t n = inline_arg_count(op);
    switch (op) {
      case 0 /* INPUT */:
      case 1 /* CONST */: {
        const std::uint16_t arg = static_cast<std::uint16_t>(bc[pc]);
        out.packed.push_back({op, 0, arg});
        break;
      }
      case 21 /* CUMSUM */: {
        const std::size_t off = static_cast<std::size_t>(bc[pc]);
        reserve_state(off, 2, 0.0);
        out.packed.push_back({op, 0, static_cast<std::uint16_t>(off)});
        break;
      }
      case 22 /* COUNT */: {
        const std::size_t off = static_cast<std::size_t>(bc[pc]);
        reserve_state(off, 1, 0.0);
        out.packed.push_back({op, 0, static_cast<std::uint16_t>(off)});
        break;
      }
      case 23 /* MAX_AGG */: {
        const std::size_t off = static_cast<std::size_t>(bc[pc]);
        reserve_state(off, 1, kNegInf);
        out.packed.push_back({op, 0, static_cast<std::uint16_t>(off)});
        break;
      }
      case 24 /* MIN_AGG */: {
        const std::size_t off = static_cast<std::size_t>(bc[pc]);
        reserve_state(off, 1, kPosInf);
        out.packed.push_back({op, 0, static_cast<std::uint16_t>(off)});
        break;
      }
      case 25 /* STATE_LOAD */: {
        const std::size_t off = static_cast<std::size_t>(bc[pc]);
        out.packed.push_back({op, 0, static_cast<std::uint16_t>(off)});
        break;
      }
      case 38 /* DIFF */:
      case 39 /* SIGN_CHANGE */: {
        const std::size_t off = static_cast<std::size_t>(bc[pc]);
        reserve_state(off, 2, 0.0);
        out.packed.push_back({op, 0, static_cast<std::uint16_t>(off)});
        break;
      }
      case 35 /* MA_UPDATE */:
      case 36 /* MSUM_UPDATE */: {
        const std::size_t W = static_cast<std::size_t>(bc[pc]);
        const std::size_t off = state_cursor;
        state_cursor += W + 3;
        reserve_state(off, W + 3, 0.0);
        const std::uint16_t aux_idx =
            static_cast<std::uint16_t>(out.aux_args.size());
        out.aux_args.push_back({static_cast<std::uint16_t>(off),
                                 static_cast<std::uint16_t>(W), 0, 0});
        out.packed.push_back({op, 0, aux_idx});
        break;
      }
      case 37 /* STD_UPDATE */: {
        const std::size_t W = static_cast<std::size_t>(bc[pc]);
        const std::size_t off = state_cursor;
        state_cursor += W + 4;
        reserve_state(off, W + 4, 0.0);
        const std::uint16_t aux_idx =
            static_cast<std::uint16_t>(out.aux_args.size());
        out.aux_args.push_back({static_cast<std::uint16_t>(off),
                                 static_cast<std::uint16_t>(W), 0, 0});
        out.packed.push_back({op, 0, aux_idx});
        break;
      }
      case 40 /* WIN_MIN */:
      case 41 /* WIN_MAX */: {
        const std::size_t W = static_cast<std::size_t>(bc[pc]);
        const std::size_t off = state_cursor;
        state_cursor += 2 * W + 2;
        reserve_state(off, 2 * W + 2, 0.0);
        const std::uint16_t aux_idx =
            static_cast<std::uint16_t>(out.aux_args.size());
        out.aux_args.push_back({static_cast<std::uint16_t>(off),
                                 static_cast<std::uint16_t>(W), 0, 0});
        out.packed.push_back({op, 0, aux_idx});
        break;
      }
      case 42 /* FIR_UPDATE */: {
        const std::size_t coeff_start = static_cast<std::size_t>(bc[pc]);
        const std::size_t coeff_len = static_cast<std::size_t>(bc[pc + 1]);
        const std::size_t W = coeff_len;
        const std::size_t off = state_cursor;
        state_cursor += W + 2;
        reserve_state(off, W + 2, 0.0);
        const std::uint16_t aux_idx =
            static_cast<std::uint16_t>(out.aux_args.size());
        out.aux_args.push_back({static_cast<std::uint16_t>(off),
                                 static_cast<std::uint16_t>(W),
                                 static_cast<std::uint16_t>(coeff_start),
                                 static_cast<std::uint16_t>(coeff_len)});
        out.packed.push_back({op, 0, aux_idx});
        break;
      }
      case 43 /* IIR_UPDATE */: {
        const std::size_t b_len = static_cast<std::size_t>(bc[pc]);
        const std::size_t a_len = static_cast<std::size_t>(bc[pc + 1]);
        const std::size_t coeff_start = static_cast<std::size_t>(bc[pc + 2]);
        const std::size_t off = state_cursor;
        state_cursor += b_len + a_len + 4;
        reserve_state(off, b_len + a_len + 4, 0.0);
        const std::uint16_t aux_idx =
            static_cast<std::uint16_t>(out.aux_args.size());
        out.aux_args.push_back({static_cast<std::uint16_t>(off),
                                 static_cast<std::uint16_t>(b_len),
                                 static_cast<std::uint16_t>(a_len),
                                 static_cast<std::uint16_t>(coeff_start)});
        out.packed.push_back({op, 0, aux_idx});
        break;
      }
      default: {
        out.packed.push_back({op, 0, 0});
        break;
      }
    }
    pc += n;
  }
  return out;
}

// Inverse of pack_bytecode: reconstruct the caller-facing bytecode stream
// from packed instructions + aux_args. Used by JSON serialization and tests.
// Windowed opcodes emit their public inline args (W, or coeff pointers) —
// internal state offsets do NOT appear in the output.
inline std::vector<double> unpack_bytecode(
    const std::vector<Instruction>& ins,
    const std::vector<AuxArgs>& aux) {
  std::vector<double> out;
  out.reserve(ins.size() * 2);
  for (auto i : ins) {
    out.push_back(static_cast<double>(i.op));
    const std::size_t n = inline_arg_count(i.op);
    if (n == 0) continue;
    switch (i.op) {
      case 35 /* MA_UPDATE */:
      case 36 /* MSUM_UPDATE */:
      case 37 /* STD_UPDATE */:
      case 40 /* WIN_MIN */:
      case 41 /* WIN_MAX */: {
        out.push_back(static_cast<double>(aux[i.arg].b));
        break;
      }
      case 42 /* FIR_UPDATE */: {
        out.push_back(static_cast<double>(aux[i.arg].c));
        out.push_back(static_cast<double>(aux[i.arg].d));
        break;
      }
      case 43 /* IIR_UPDATE */: {
        out.push_back(static_cast<double>(aux[i.arg].b));
        out.push_back(static_cast<double>(aux[i.arg].c));
        out.push_back(static_cast<double>(aux[i.arg].d));
        break;
      }
      default: {
        out.push_back(static_cast<double>(i.arg));
        break;
      }
    }
  }
  return out;
}

}  // namespace rtbot::fuse

#endif  // RTBOT_FUSE_BYTECODE_H
