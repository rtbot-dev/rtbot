#include <catch2/catch.hpp>

#include "rtbot/std/And.h"

using namespace rtbot;
using namespace std;

TEST_CASE("And join") {
  OperatorPayload<uint64_t, double> emitted;
  auto andL = And<uint64_t, double>("and");

  andL.receiveData(Message<uint64_t, double>(1, 1), "i1");
  andL.receiveData(Message<uint64_t, double>(2, 1), "i1");
  andL.receiveData(Message<uint64_t, double>(3, 0), "i1");
  andL.receiveData(Message<uint64_t, double>(4, 1), "i1");

  andL.receiveData(Message<uint64_t, double>(2, 1), "i2");
  emitted = andL.executeData();

  REQUIRE(emitted.find("and")->second.find("o1")->second.at(0).value == 1);

  andL.receiveData(Message<uint64_t, double>(4, 0), "i2");
  emitted = andL.executeData();

  REQUIRE(emitted.find("and")->second.find("o1")->second.at(0).value == 0);

  andL.receiveData(Message<uint64_t, double>(3, 1), "i2");
  emitted = andL.executeData();

  REQUIRE(emitted.empty());
}
