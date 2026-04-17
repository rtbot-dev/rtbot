#ifndef RTBOT_FUSE_AUX_ARGS_H
#define RTBOT_FUSE_AUX_ARGS_H

#include <cstdint>
#include <type_traits>

namespace rtbot::fuse {

// Side-table record for opcodes that carry more than one packed inline arg.
//
// Instruction has a single uint16_t arg; windowed/DSP opcodes need up to four
// fields (state offset, window size, coefficient offset, coefficient length).
// For those, Instruction::arg becomes an index into std::vector<AuxArgs> held
// by the operator. Scalar opcodes like ADD/MUL/CUMSUM/COUNT keep using
// Instruction::arg directly and don't touch the side table.
//
// Layout is fixed-size 4×uint16_t (8 bytes) so cache lines hold 8 records.
struct AuxArgs {
  std::uint16_t a;
  std::uint16_t b;
  std::uint16_t c;
  std::uint16_t d;
};

static_assert(sizeof(AuxArgs) == 8, "AuxArgs must be 8 bytes");
static_assert(std::is_trivially_copyable_v<AuxArgs>,
              "AuxArgs must be trivially copyable");

}  // namespace rtbot::fuse

#endif  // RTBOT_FUSE_AUX_ARGS_H
