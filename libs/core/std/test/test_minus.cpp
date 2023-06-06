#include <catch2/catch.hpp>

#include "rtbot/std/Minus.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Minus joint") {
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

TEST_CASE("Minus joint i1 eager") {
  map<string, std::vector<Message<std::uint64_t, double>>> emitted;
  auto minus = Minus<std::uint64_t, double>("minus", {{"i1", Operator<uint64_t, double>::InputPolicy(true)}});

  minus.receive(Message<uint64_t, double>(1, 1), "i1");
  minus.receive(Message<uint64_t, double>(2, 2), "i1");
  minus.receive(Message<uint64_t, double>(3, 3), "i1");
  minus.receive(Message<uint64_t, double>(4, 4), "i1");

  emitted = minus.receive(Message<uint64_t, double>(2, 3), "i2");

  REQUIRE(emitted.find("minus")->second.at(0).value == 1);
  REQUIRE(emitted.find("minus")->second.at(0).time == 2);

  emitted = minus.receive(Message<uint64_t, double>(3, 4), "i2");

  REQUIRE(emitted.find("minus")->second.at(0).value == 0);
  REQUIRE(emitted.find("minus")->second.at(0).time == 3);

  emitted = minus.receive(Message<uint64_t, double>(5, 5), "i2");

  REQUIRE(emitted.find("minus")->second.at(0).value == -1);
  REQUIRE(emitted.find("minus")->second.at(0).time == 5);
}
