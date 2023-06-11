#include <catch2/catch.hpp>

#include "rtbot/std/Identity.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Identity") {
  auto id = Identity<std::uint64_t, double>("id");
  auto idDelayed = Identity<std::uint64_t, double>("idd", 1);

  SECTION("emits Identity no delay") {
    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = id.receive(Message<std::uint64_t, double>(i, i));
      REQUIRE(emitted.find("id")->second.at(0).value == i);
      REQUIRE(emitted.find("id")->second.at(0).time == i);
    }
  }

  SECTION("emits Identity delayed 1") {
    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = idDelayed.receive(Message<std::uint64_t, double>(i, i));
      if (i == 1)
        REQUIRE(emitted.empty());
      else {
        REQUIRE(emitted.find("idd")->second.at(0).value == i - 1);
        REQUIRE(emitted.find("idd")->second.at(0).time == i - 1);
      }
    }
  }
}
