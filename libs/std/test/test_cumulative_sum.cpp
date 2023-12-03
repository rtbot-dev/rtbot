#include <catch2/catch.hpp>

#include "rtbot/std/CumulativeSum.h"

using namespace rtbot;
using namespace std;

TEST_CASE("CumulativeSum test") {
  auto cs = CumulativeSum<uint64_t, double>("cs");

  SECTION("emits  sum") {
    map<string, map<string, vector<Message<uint64_t, double>>>> emitted;
    for (int i = 1; i <= 50; i++) {
      cs.receiveData(Message<uint64_t, double>(i, i));
      emitted = cs.executeData();
      REQUIRE(emitted.find("cs")->second.find("o1")->second.at(0).value == i * (i + 1) / 2);
    }
  }
}
