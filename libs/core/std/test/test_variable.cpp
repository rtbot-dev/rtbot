#include <catch2/catch.hpp>

#include "rtbot/std/Variable.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Variable sync") {
  map<string, map<string, vector<Message<uint64_t, double>>>> emitted;
  auto variable = Variable<uint64_t, double>("variable");

  variable.receiveData(Message<uint64_t, double>(1, 1.1), "i1");
  variable.receiveData(Message<uint64_t, double>(3, 1.2), "i1");
  variable.receiveData(Message<uint64_t, double>(5, 2.5), "i1");
  variable.receiveData(Message<uint64_t, double>(7, 2.8), "i1");
  emitted = variable.receiveControl(Message<uint64_t, double>(7, 1), "c1");

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 2.8);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 7);

  variable.receiveControl(Message<uint64_t, double>(9, 1), "c1");
  emitted = variable.receiveData(Message<uint64_t, double>(8, 2.8), "i1");

  REQUIRE(emitted.empty());
}
