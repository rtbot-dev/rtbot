#include <catch2/catch.hpp>

#include "rtbot/std/Linear.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Linear joint no eager") {
  map<string, vector<Message<uint64_t, double>>> emitted;
  auto linear = Linear<uint64_t, double>("linear", {2, -1});

  linear.receive(Message<uint64_t, double>(1, 1), "i1");
  linear.receive(Message<uint64_t, double>(2, 2), "i1");
  linear.receive(Message<uint64_t, double>(3, 3), "i1");
  linear.receive(Message<uint64_t, double>(4, 4), "i1");

  emitted = linear.receive(Message<uint64_t, double>(2, 3), "i2");

  REQUIRE(emitted.find("linear")->second.at(0).value == 1);

  emitted = linear.receive(Message<uint64_t, double>(4, 4), "i2");

  REQUIRE(emitted.find("linear")->second.at(0).value == 4);

  emitted = linear.receive(Message<uint64_t, double>(5, 5), "i1");

  REQUIRE(emitted.empty());
}

TEST_CASE("Linear joint i2 eager") {
  map<string, vector<Message<uint64_t, double>>> emitted;
  auto linear = Linear<uint64_t, double>("linear", {2, -1}, {{"i2", Operator<uint64_t, double>::InputPolicy(true)}});

  linear.receive(Message<uint64_t, double>(1, 1), "i1");
  linear.receive(Message<uint64_t, double>(2, 2), "i1");
  linear.receive(Message<uint64_t, double>(3, 3), "i1");
  linear.receive(Message<uint64_t, double>(4, 4), "i1");

  REQUIRE(!linear.isEager("i1"));
  REQUIRE(linear.isEager("i2"));

  emitted = linear.receive(Message<uint64_t, double>(2, 3), "i2");

  REQUIRE(emitted.find("linear")->second.at(0).value == -1);
  /* get the time of the non eager port*/
  REQUIRE(emitted.find("linear")->second.at(0).time == 1);

  emitted = linear.receive(Message<uint64_t, double>(4, 4), "i2");

  REQUIRE(emitted.find("linear")->second.at(0).value == 0);
  /* get the time of the non eager port*/
  REQUIRE(emitted.find("linear")->second.at(0).time == 2);

  emitted = linear.receive(Message<uint64_t, double>(5, 5), "i1");

  REQUIRE(emitted.find("linear")->second.at(0).value == 2);
  /* get the time of the non eager port*/
  REQUIRE(emitted.find("linear")->second.at(0).time == 3);
}
