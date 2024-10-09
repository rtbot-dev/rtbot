#include <catch2/catch.hpp>

#include "rtbot/std/GreaterThanStream.h"

using namespace rtbot;
using namespace std;

TEST_CASE("GreaterThanStream join") {
  OperatorPayload<uint64_t, double> emitted;
  auto gts = GreaterThanStream<uint64_t, double>("gts");

  gts.receiveData(Message<uint64_t, double>(1, 1), "i1");
  gts.receiveData(Message<uint64_t, double>(2, 1), "i1");
  gts.receiveData(Message<uint64_t, double>(3, 0), "i1");
  gts.receiveData(Message<uint64_t, double>(4, 2), "i1");
  gts.receiveData(Message<uint64_t, double>(5, 0.3), "i1");
  gts.receiveData(Message<uint64_t, double>(6, 1), "i1");
  gts.receiveData(Message<uint64_t, double>(7, 3), "i1");

  gts.receiveData(Message<uint64_t, double>(2, 1), "i2");
  emitted = gts.executeData();
  REQUIRE(emitted.empty());

  gts.receiveData(Message<uint64_t, double>(4, 1), "i2");
  emitted = gts.executeData();
  REQUIRE(emitted.find("gts")->second.find("o1")->second.at(0).value == 2);

  gts.receiveData(Message<uint64_t, double>(5, 0), "i2");
  emitted = gts.executeData();
  REQUIRE(emitted.find("gts")->second.find("o1")->second.at(0).value == 0.3);

  gts.receiveData(Message<uint64_t, double>(6, 1), "i2");
  emitted = gts.executeData();
  REQUIRE(emitted.empty());

  gts.receiveData(Message<uint64_t, double>(7, 2), "i2");
  emitted = gts.executeData();
  REQUIRE(emitted.find("gts")->second.find("o1")->second.at(0).value == 3);
}
