#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/std/PartialSum.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Partial Sum") {
  map<string, std::vector<Message<std::uint64_t, double>>> emitted;
  auto ps = PartialSum<std::uint64_t, double>("ps");

  SECTION("emits partial sum") {
    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = ps.receive(Message<std::uint64_t, double>(i, i));
      REQUIRE(emitted.find("ps")->second.at(0).value == i * (i + 1) / 2);
    }
  }
}
