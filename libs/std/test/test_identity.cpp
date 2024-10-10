#include <catch2/catch.hpp>

#include "rtbot/std/Identity.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Identity") {
  auto id = Identity<uint64_t, double>("id");
  auto idDelayed = Identity<uint64_t, double>("idd");

  SECTION("emits Identity no delay") {
    ProgramMessage<uint64_t, double> emitted;
    for (int i = 1; i <= 50; i++) {
      id.receiveData(Message<uint64_t, double>(i, i));
      emitted = id.executeData();
      REQUIRE(emitted.find("id")->second.find("o1")->second.at(0).value == i);
      REQUIRE(emitted.find("id")->second.find("o1")->second.at(0).time == i);
    }
  }
}
