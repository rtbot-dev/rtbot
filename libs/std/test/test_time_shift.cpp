#include <catch2/catch.hpp>

#include "rtbot/std/TimeShift.h"

using namespace rtbot;
using namespace std;

TEST_CASE("TimeShift") {
  auto ts = TimeShift<uint64_t, double>("ts", 2, 2);

  SECTION("emits shifted 2") {
    OperatorPayload<uint64_t, double> emitted;
    for (int i = 1; i <= 50; i++) {
      ts.receiveData(Message<uint64_t, double>(i, i));
      emitted = ts.executeData();
      REQUIRE(emitted.find("ts")->second.find("o1")->second.at(0).value == i);
      REQUIRE(emitted.find("ts")->second.find("o1")->second.at(0).time == i + (2 * 2));
    }
  }
}