#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/std/LessThan.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Less Than") {
  auto lt = LessThan<std::uint64_t, double>("lt", 0.4);

  SECTION("only one emission") {
    int t = 0;
    int sign = 1;
    double v = 0.0;

    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 11; i++) {
      t++;
      v += sign * 0.1;
      if (t % 6 == 0) sign = -sign;
      emitted = lt.receive(Message<std::uint64_t, double>(i, v));
      if (i < 4 || i > 8) {
        REQUIRE(emitted.find("lt")->second.at(0).value == v);
        REQUIRE(emitted.find("lt")->second.at(0).time == i);
        REQUIRE(emitted.find("lt")->second.size() == 1);
      } else if (i == 4 || i == 5 || i == 6 || i == 7) {
        REQUIRE(emitted.empty());
      }
    }
  }
}
