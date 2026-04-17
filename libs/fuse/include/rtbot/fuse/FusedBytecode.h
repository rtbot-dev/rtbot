#ifndef RTBOT_FUSE_BYTECODE_H
#define RTBOT_FUSE_BYTECODE_H

#include <cstdint>
#include <type_traits>
#include <vector>

#include "rtbot/fuse/FusedOps.h"

namespace rtbot::fuse {

// Packed bytecode record for FusedExpression programs.
//
// Replaces the earlier `std::vector<double>` encoding (16 bytes per opcode+arg)
// with a 4-byte record. Opcodes fit in uint8_t (projected max ~45). `arg` is
// the inline argument consumed by INPUT/CONST/stateful opcodes; for opcodes
// with no inline arg the field is unused. `flags` is reserved for Phase 3+
// (emission-semantics marker on windowed opcodes) and Phase 4 (debug marks).
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

// Opcodes that consume the following double as an inline argument in the
// legacy bytecode format. Kept in sync with FusedExpression's validator and
// evaluate_one switch semantics. Tier-1 windowed opcodes (35-43) all carry
// exactly one inline arg: either a state offset (DIFF, SIGN_CHANGE) or an
// index into the operator's AuxArgs side table (everything else).
inline constexpr bool has_inline_arg(std::uint8_t op) {
  using namespace rtbot::fused_op;
  return op == static_cast<std::uint8_t>(INPUT) ||
         op == static_cast<std::uint8_t>(CONST) ||
         op == static_cast<std::uint8_t>(CUMSUM) ||
         op == static_cast<std::uint8_t>(COUNT) ||
         op == static_cast<std::uint8_t>(MAX_AGG) ||
         op == static_cast<std::uint8_t>(MIN_AGG) ||
         op == static_cast<std::uint8_t>(STATE_LOAD) ||
         (op >= static_cast<std::uint8_t>(MA_UPDATE) &&
          op <= static_cast<std::uint8_t>(IIR_UPDATE));
}

// Convert a double-encoded bytecode stream to the packed Instruction form.
// Preserves opcode order; inline args become the `arg` field of the same
// instruction. This is the input side of the caller-facing bytecode API:
// compilers and tests build programs as `std::vector<double>`, operators
// store them internally as `std::vector<Instruction>`.
inline std::vector<Instruction> pack_bytecode(const std::vector<double>& bc) {
  std::vector<Instruction> out;
  out.reserve(bc.size());
  std::size_t pc = 0;
  while (pc < bc.size()) {
    std::uint8_t op = static_cast<std::uint8_t>(bc[pc++]);
    std::uint16_t arg = 0;
    if (has_inline_arg(op)) {
      arg = static_cast<std::uint16_t>(bc[pc++]);
    }
    out.push_back({op, 0, arg});
  }
  return out;
}

// Inverse of pack_bytecode — used by JSON serialization and test helpers
// that want to round-trip a program back to the caller-facing double form.
inline std::vector<double> unpack_bytecode(const std::vector<Instruction>& ins) {
  std::vector<double> out;
  out.reserve(ins.size() * 2);
  for (auto i : ins) {
    out.push_back(static_cast<double>(i.op));
    if (has_inline_arg(i.op)) out.push_back(static_cast<double>(i.arg));
  }
  return out;
}

}  // namespace rtbot::fuse

#endif  // RTBOT_FUSE_BYTECODE_H
