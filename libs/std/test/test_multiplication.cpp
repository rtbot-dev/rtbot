#include <catch2/catch.hpp>

#include "rtbot/std/Multiplication.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Multiplication join") {
  OperatorPayload<uint64_t, double> emitted;
  auto mult = Multiplication<uint64_t, double>("mult");

  mult.receiveData(Message<uint64_t, double>(1, 1), "i1");
  mult.receiveData(Message<uint64_t, double>(2, 2), "i1");
  mult.receiveData(Message<uint64_t, double>(3, 3), "i1");
  mult.receiveData(Message<uint64_t, double>(4, 4), "i1");

  mult.receiveData(Message<uint64_t, double>(2, 3), "i2");
  emitted = mult.executeData();

  REQUIRE(emitted.find("mult")->second.find("o1")->second.at(0).value == 6);

  mult.receiveData(Message<uint64_t, double>(4, 4), "i2");
  emitted = mult.executeData();

  REQUIRE(emitted.find("mult")->second.find("o1")->second.at(0).value == 16);

  mult.receiveData(Message<uint64_t, double>(3, 5), "i2");
  emitted = mult.executeData();

  REQUIRE(emitted.empty());
}
