#include <catch2/catch.hpp>

#include "rtbot/std/Divide.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Divide joint") {
  map<string, map<string, vector<Message<uint64_t, double>>>> emitted;
  auto divide = Divide<uint64_t, double>("divide");

  divide.receiveData(Message<uint64_t, double>(1, 1), "i1");
  divide.receiveData(Message<uint64_t, double>(2, 6), "i1");
  divide.receiveData(Message<uint64_t, double>(3, 3), "i1");
  divide.receiveData(Message<uint64_t, double>(4, 4), "i1");

  divide.receiveData(Message<uint64_t, double>(2, 3), "i2");
  emitted = divide.executeData();

  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(0).value == 2);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(0).time == 2);

  divide.receiveData(Message<uint64_t, double>(4, 4), "i2");
  emitted = divide.executeData();

  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(0).value == 1);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(0).time == 4);

  divide.receiveData(Message<uint64_t, double>(3, 5), "i2");
  emitted = divide.executeData();

  REQUIRE(emitted.empty());
}

TEST_CASE("Divide joint i2 eager") {
  map<string, map<string, vector<Message<uint64_t, double>>>> emitted;
  auto divide = Divide<uint64_t, double>("divide", {{"i2", Operator<uint64_t, double>::InputPolicy(true)}});

  divide.receiveData(Message<uint64_t, double>(1, 1), "i1");
  divide.receiveData(Message<uint64_t, double>(2, 6), "i1");
  divide.receiveData(Message<uint64_t, double>(3, 3), "i1");
  divide.receiveData(Message<uint64_t, double>(4, 4), "i1");

  divide.receiveData(Message<uint64_t, double>(2, 2), "i2");
  emitted = divide.executeData();

  REQUIRE(emitted.find("divide")->second.find("o1")->second.size() == 4);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(0).value == 0.5);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(0).time == 1);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(1).value == 3);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(1).time == 2);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(2).value == 1.5);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(2).time == 3);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(3).value == 2);
  REQUIRE(emitted.find("divide")->second.find("o1")->second.at(3).time == 4);

  divide.receiveData(Message<uint64_t, double>(2, 2), "i2");
  emitted = divide.executeData();

  REQUIRE(emitted.empty());
}
