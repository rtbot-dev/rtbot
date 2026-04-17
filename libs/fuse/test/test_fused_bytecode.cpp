#include <catch2/catch.hpp>

#include <type_traits>
#include <vector>

#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedExpression.h"  // for fused_op namespace constants

using rtbot::fuse::unpack_bytecode;
using rtbot::fuse::pack_bytecode;
using rtbot::fuse::Instruction;
using namespace rtbot::fused_op;

SCENARIO("Instruction is a 4-byte POD", "[fused_bytecode]") {
  REQUIRE(sizeof(Instruction) == 4);
  REQUIRE(std::is_trivially_copyable_v<Instruction>);
  REQUIRE(std::is_standard_layout_v<Instruction>);
}

SCENARIO("Instruction fields pack correctly", "[fused_bytecode]") {
  Instruction i{7, 0x01, 0xBEEF};
  REQUIRE(i.op == 7);
  REQUIRE(i.flags == 0x01);
  REQUIRE(i.arg == 0xBEEF);
}

SCENARIO("pack_bytecode converts double bytecode to packed",
         "[fused_bytecode][adapter]") {
  std::vector<double> source = {INPUT, 0, INPUT, 1, ADD, END};
  auto pack = pack_bytecode(source);
  const auto& packed = pack.packed;
  REQUIRE(packed.size() == 4);
  REQUIRE(packed[0].op == static_cast<std::uint8_t>(INPUT));
  REQUIRE(packed[0].arg == 0);
  REQUIRE(packed[1].op == static_cast<std::uint8_t>(INPUT));
  REQUIRE(packed[1].arg == 1);
  REQUIRE(packed[2].op == static_cast<std::uint8_t>(ADD));
  REQUIRE(packed[2].arg == 0);
  REQUIRE(packed[3].op == static_cast<std::uint8_t>(END));
}

SCENARIO("pack_bytecode and unpack_bytecode roundtrip",
         "[fused_bytecode][adapter]") {
  std::vector<double> original = {INPUT, 2, CONST, 5, POW, END,
                                    CUMSUM, 0, END};
  auto pack = pack_bytecode(original);
  auto roundtrip = unpack_bytecode(pack.packed, pack.aux_args);
  REQUIRE(roundtrip == original);
}

SCENARIO("Stateful opcodes carry inline args in packed form",
         "[fused_bytecode][adapter]") {
  std::vector<double> source = {INPUT, 0, MAX_AGG, 3, END,
                                  COUNT, 7, END,
                                  STATE_LOAD, 2, END};
  auto pack = pack_bytecode(source);
  const auto& packed = pack.packed;
  // 3 opcodes with inline args (MAX_AGG, COUNT, STATE_LOAD) + 1 INPUT + 3 ENDs
  //   = 7 instructions total
  REQUIRE(packed.size() == 7);
  REQUIRE(packed[1].op == static_cast<std::uint8_t>(MAX_AGG));
  REQUIRE(packed[1].arg == 3);
  REQUIRE(packed[3].op == static_cast<std::uint8_t>(COUNT));
  REQUIRE(packed[3].arg == 7);
  REQUIRE(packed[5].op == static_cast<std::uint8_t>(STATE_LOAD));
  REQUIRE(packed[5].arg == 2);
}

SCENARIO("pack_bytecode auto-allocates state and aux_args for windowed opcodes",
         "[fused_bytecode][adapter]") {
  // INPUT 0, MA_UPDATE window=10, END — the MA_UPDATE carries W inline.
  std::vector<double> source = {INPUT, 0, MA_UPDATE, 10, END};
  auto pack = pack_bytecode(source);
  REQUIRE(pack.packed.size() == 3);
  REQUIRE(pack.packed[1].op == static_cast<std::uint8_t>(MA_UPDATE));
  // aux_args[0] built from the inline W:
  //   {state_off=0, window=10, _=0, _=0}.
  REQUIRE(pack.aux_args.size() == 1);
  REQUIRE(pack.aux_args[0].a == 0);
  REQUIRE(pack.aux_args[0].b == 10);
  // state_init sized to ring(10) + kahan_sum + kahan_comp + count = 13.
  REQUIRE(pack.state_init.size() == 13);
  // Round-trip preserves the public inline-arg form.
  auto roundtrip = unpack_bytecode(pack.packed, pack.aux_args);
  REQUIRE(roundtrip == source);
}
