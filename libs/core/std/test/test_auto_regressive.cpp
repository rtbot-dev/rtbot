#include <math.h>

#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/std/AutoRegressive.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Auto Regressive") {
  auto i1 = AutoRegressive<std::uint64_t, double>("i1", {1, 1, 1, 1, 1, 1});

  SECTION("emits powers of 2") {
    for (int i = 1; i <= 6; i++) {
      map<string, std::vector<Message<std::uint64_t, double>>> emitted =
          i1.receive(Message<std::uint64_t, double>(i, 1));
      REQUIRE(emitted.find("i1")->second.at(0).value == pow(2, i - 1));
    }
  }
}
