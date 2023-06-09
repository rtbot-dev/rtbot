#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/std/Accumulator.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Accumulator test") {
  auto ps = Accumulator<uint64_t, double>("ac");

  SECTION("emits  sum") {
    map<string, std::vector<Message<uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = ps.receive(Message<uint64_t, double>(i, i));
      REQUIRE(emitted.find("ac")->second.at(0).value == i * (i + 1) / 2);
    }
  }
}
