#include <catch2/catch.hpp>

#include "rtbot/std/Division.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Division join") {
  map<string, map<string, vector<Message<uint64_t, double>>>> emitted;
  auto division = Division<uint64_t, double>("division");

  division.receiveData(Message<uint64_t, double>(1, 1), "i1");
  division.receiveData(Message<uint64_t, double>(2, 6), "i1");
  division.receiveData(Message<uint64_t, double>(3, 3), "i1");
  division.receiveData(Message<uint64_t, double>(4, 4), "i1");

  division.receiveData(Message<uint64_t, double>(2, 3), "i2");
  emitted = division.executeData();

  REQUIRE(emitted.find("division")->second.find("o1")->second.at(0).value == 2);
  REQUIRE(emitted.find("division")->second.find("o1")->second.at(0).time == 2);

  division.receiveData(Message<uint64_t, double>(4, 4), "i2");
  emitted = division.executeData();

  REQUIRE(emitted.find("division")->second.find("o1")->second.at(0).value == 1);
  REQUIRE(emitted.find("division")->second.find("o1")->second.at(0).time == 4);

  division.receiveData(Message<uint64_t, double>(3, 5), "i2");
  emitted = division.executeData();

  REQUIRE(emitted.empty());
}
