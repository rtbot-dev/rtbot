#include <catch2/catch.hpp>

#include "rtbot/std/Power.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Power") {
  auto pow = Power<uint64_t, double>("pow", -1);

  SECTION("emits Power -1") {
    map<string, vector<Message<uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = pow.receiveData(Message<uint64_t, double>(i, i * 2));
      REQUIRE(emitted.find("pow")->second.at(0).value == 1 / ((double)(i * 2)));
      REQUIRE(emitted.find("pow")->second.at(0).time == i);
    }
  }
}
