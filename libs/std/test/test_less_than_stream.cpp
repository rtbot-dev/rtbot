#include <catch2/catch.hpp>

#include "rtbot/std/LessThanStream.h"

using namespace rtbot;
using namespace std;

TEST_CASE("LessThanStream join") {
  ProgramMessage<uint64_t, double> emitted;
  auto lts = LessThanStream<uint64_t, double>("lts");

  lts.receiveData(Message<uint64_t, double>(1, 1), "i1");
  lts.receiveData(Message<uint64_t, double>(2, 1), "i1");
  lts.receiveData(Message<uint64_t, double>(3, 0), "i1");
  lts.receiveData(Message<uint64_t, double>(4, 1), "i1");
  lts.receiveData(Message<uint64_t, double>(5, 0), "i1");
  lts.receiveData(Message<uint64_t, double>(6, 2), "i1");
  lts.receiveData(Message<uint64_t, double>(7, -1), "i1");

  lts.receiveData(Message<uint64_t, double>(2, 2), "i2");
  emitted = lts.executeData();
  REQUIRE(emitted.find("lts")->second.find("o1")->second.at(0).value == 1);

  lts.receiveData(Message<uint64_t, double>(4, 2), "i2");
  emitted = lts.executeData();
  REQUIRE(emitted.find("lts")->second.find("o1")->second.at(0).value == 1);

  lts.receiveData(Message<uint64_t, double>(5, 0.1), "i2");
  emitted = lts.executeData();
  REQUIRE(emitted.find("lts")->second.find("o1")->second.at(0).value == 0);

  lts.receiveData(Message<uint64_t, double>(6, 1), "i2");
  emitted = lts.executeData();
  REQUIRE(emitted.empty());

  lts.receiveData(Message<uint64_t, double>(7, 0), "i2");
  emitted = lts.executeData();
  REQUIRE(emitted.find("lts")->second.find("o1")->second.at(0).value == -1);
}
