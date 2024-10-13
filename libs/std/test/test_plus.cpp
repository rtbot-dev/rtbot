#include <catch2/catch.hpp>

#include "rtbot/std/Plus.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Plus join") {
  ProgramMessage<uint64_t, double> emitted;
  auto plus = Plus<uint64_t, double>("plus");

  plus.receiveData(Message<uint64_t, double>(1, 1), "i1");
  plus.receiveData(Message<uint64_t, double>(2, 2), "i1");
  plus.receiveData(Message<uint64_t, double>(3, 3), "i1");
  plus.receiveData(Message<uint64_t, double>(4, 4), "i1");

  plus.receiveData(Message<uint64_t, double>(2, 3), "i2");
  emitted = plus.executeData();

  REQUIRE(emitted.find("plus")->second.find("o1")->second.at(0).value == 5);

  plus.receiveData(Message<uint64_t, double>(4, 4), "i2");
  emitted = plus.executeData();

  REQUIRE(emitted.find("plus")->second.find("o1")->second.at(0).value == 8);

  plus.receiveData(Message<uint64_t, double>(3, 5), "i2");
  emitted = plus.executeData();

  REQUIRE(emitted.empty());
}
