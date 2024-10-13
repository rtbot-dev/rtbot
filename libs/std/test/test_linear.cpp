#include <catch2/catch.hpp>

#include "rtbot/std/Linear.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Linear joint no eager") {
  ProgramMessage<uint64_t, double> emitted;
  auto linear = Linear<uint64_t, double>("linear", {2, -1});

  linear.receiveData(Message<uint64_t, double>(1, 1), "i1");
  linear.receiveData(Message<uint64_t, double>(2, 2), "i1");
  linear.receiveData(Message<uint64_t, double>(3, 3), "i1");
  linear.receiveData(Message<uint64_t, double>(4, 4), "i1");

  linear.receiveData(Message<uint64_t, double>(2, 3), "i2");
  emitted = linear.executeData();

  REQUIRE(emitted.find("linear")->second.find("o1")->second.at(0).value == 1);

  linear.receiveData(Message<uint64_t, double>(4, 4), "i2");
  emitted = linear.executeData();

  REQUIRE(emitted.find("linear")->second.find("o1")->second.at(0).value == 4);

  linear.receiveData(Message<uint64_t, double>(5, 5), "i1");
  emitted = linear.executeData();

  REQUIRE(emitted.empty());
}