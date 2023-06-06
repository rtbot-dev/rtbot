#include <catch2/catch.hpp>

#include "rtbot/std/Minus.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Minus join") {
  map<string, std::vector<Message<std::uint64_t, double>>> emitted;
  auto minus = Minus<std::uint64_t, double>("minus");

  minus.receive(Message<uint64_t, double>(1, 1), "i1");
  minus.receive(Message<uint64_t, double>(2, 2), "i1");
  minus.receive(Message<uint64_t, double>(3, 3), "i1");
  minus.receive(Message<uint64_t, double>(4, 4), "i1");

  emitted = minus.receive(Message<uint64_t, double>(2, 3), "i2");

  REQUIRE(emitted.find("minus")->second.at(0).value == -1);

  emitted = minus.receive(Message<uint64_t, double>(4, 4), "i2");

  REQUIRE(emitted.find("minus")->second.at(0).value == 0);

  emitted = minus.receive(Message<uint64_t, double>(3, 5), "i2");

  REQUIRE(emitted.empty());
}
