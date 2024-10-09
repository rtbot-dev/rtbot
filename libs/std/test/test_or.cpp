#include <catch2/catch.hpp>

#include "rtbot/std/Or.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Or join") {
  OperatorPayload<uint64_t, double> emitted;
  auto orL = Or<uint64_t, double>("or");

  orL.receiveData(Message<uint64_t, double>(1, 1), "i1");
  orL.receiveData(Message<uint64_t, double>(2, 1), "i1");
  orL.receiveData(Message<uint64_t, double>(3, 0), "i1");
  orL.receiveData(Message<uint64_t, double>(4, 1), "i1");
  orL.receiveData(Message<uint64_t, double>(5, 0), "i1");

  orL.receiveData(Message<uint64_t, double>(2, 1), "i2");
  emitted = orL.executeData();

  REQUIRE(emitted.find("or")->second.find("o1")->second.at(0).value == 1);

  orL.receiveData(Message<uint64_t, double>(4, 0), "i2");
  emitted = orL.executeData();

  REQUIRE(emitted.find("or")->second.find("o1")->second.at(0).value == 1);

  orL.receiveData(Message<uint64_t, double>(3, 1), "i2");
  emitted = orL.executeData();
  REQUIRE(emitted.empty());

  orL.receiveData(Message<uint64_t, double>(5, 0.3), "i2");
  emitted = orL.executeData();
  REQUIRE(emitted.find("or")->second.find("o1")->second.at(0).value == 0);
}
