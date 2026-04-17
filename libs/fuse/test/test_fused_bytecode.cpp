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
  std::vector<double> legacy = {INPUT, 0, INPUT, 1, ADD, END};
  auto packed = pack_bytecode(legacy);
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
  auto packed = pack_bytecode(original);
  auto roundtrip = unpack_bytecode(packed);
  REQUIRE(roundtrip == original);
}

SCENARIO("Stateful opcodes carry inline args in packed form",
         "[fused_bytecode][adapter]") {
  std::vector<double> legacy = {INPUT, 0, MAX_AGG, 3, END,
                                  COUNT, 7, END,
                                  STATE_LOAD, 2, END};
  auto packed = pack_bytecode(legacy);
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
