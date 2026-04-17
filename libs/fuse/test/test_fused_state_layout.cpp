#include <catch2/catch.hpp>

#include <limits>
#include <type_traits>

#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedStateLayout.h"

using namespace rtbot::fuse;
using namespace rtbot::fused_op;

SCENARIO("AuxArgs is a packed 4x uint16 POD", "[aux_args]") {
  REQUIRE(sizeof(AuxArgs) == 8);
  REQUIRE(std::is_trivially_copyable_v<AuxArgs>);
  AuxArgs a{1, 2, 3, 4};
  REQUIRE(a.a == 1);
  REQUIRE(a.b == 2);
  REQUIRE(a.c == 3);
  REQUIRE(a.d == 4);
}

SCENARIO("State layout reserves slots for scalar stateful opcodes",
         "[state_layout]") {
  // CUMSUM at offset 0 (2 slots) + COUNT at offset 5 (1 slot) -> total = 6.
  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(CUMSUM), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
      {static_cast<std::uint8_t>(COUNT), 0, 5},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  auto layout = compute_state_layout(packed, {});
  REQUIRE(layout.total_state_size == 6);
  REQUIRE(layout.initial_values.size() == 6);
  REQUIRE(layout.initial_values[0] == 0.0);  // CUMSUM sum
  REQUIRE(layout.initial_values[1] == 0.0);  // CUMSUM kahan
  REQUIRE(layout.initial_values[5] == 0.0);  // COUNT
}

SCENARIO("State layout seeds MIN_AGG / MAX_AGG with infinity",
         "[state_layout]") {
  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(MAX_AGG), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(MIN_AGG), 0, 1},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  auto layout = compute_state_layout(packed, {});
  REQUIRE(layout.total_state_size == 2);
  REQUIRE(layout.initial_values[0] ==
          -std::numeric_limits<double>::infinity());
  REQUIRE(layout.initial_values[1] ==
          std::numeric_limits<double>::infinity());
}

SCENARIO("State layout reserves ring+scalars for MA_UPDATE", "[state_layout]") {
  // MA_UPDATE uses aux[0] = {state_off=0, win_size=10}; reserves 13 slots.
  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(MA_UPDATE), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  std::vector<AuxArgs> aux = {{0, 10, 0, 0}};
  auto layout = compute_state_layout(packed, aux);
  REQUIRE(layout.total_state_size == 13);  // 10 ring + sum + kahan + count
  for (double v : layout.initial_values) REQUIRE(v == 0.0);
}

SCENARIO("State layout reserves extra scalar for STD_UPDATE",
         "[state_layout]") {
  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(STD_UPDATE), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  std::vector<AuxArgs> aux = {{0, 50, 0, 0}};
  auto layout = compute_state_layout(packed, aux);
  REQUIRE(layout.total_state_size == 54);  // 50 ring + mean + m2 + count + pad
}

SCENARIO("State layout reserves 2*win + 2 for WIN_MIN/MAX", "[state_layout]") {
  std::vector<Instruction> packed = {
      {static_cast<std::uint8_t>(INPUT), 0, 0},
      {static_cast<std::uint8_t>(WIN_MAX), 0, 0},
      {static_cast<std::uint8_t>(END), 0, 0},
  };
  std::vector<AuxArgs> aux = {{0, 16, 0, 0}};
  auto layout = compute_state_layout(packed, aux);
  REQUIRE(layout.total_state_size == 34);  // 16 ring + 16 deque + 2 head/tail
  REQUIRE(layout.initial_values[0] ==
          -std::numeric_limits<double>::infinity());
}
