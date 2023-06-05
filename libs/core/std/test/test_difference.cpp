#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/std/Minus.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Difference join") {
  map<string, std::vector<Message<std::uint64_t, double>>> emitted;
  auto diff = Minus<std::uint64_t, double>("diff");

  diff.receive(Message<uint64_t, double>(1, 1), "i1");
  diff.receive(Message<uint64_t, double>(2, 2), "i1");
  diff.receive(Message<uint64_t, double>(3, 3), "i1");
  diff.receive(Message<uint64_t, double>(4, 4), "i1");

  emitted = diff.receive(Message<uint64_t, double>(2, 3), "i2");

  REQUIRE(emitted.find("diff")->second.at(0).value == -1);

  emitted = diff.receive(Message<uint64_t, double>(4, 4), "i2");

  REQUIRE(emitted.find("diff")->second.at(0).value == 0);

  emitted = diff.receive(Message<uint64_t, double>(5, 5), "i1");

  REQUIRE(emitted.empty());
}
