#include <catch2/catch.hpp>

#include "rtbot/std/Delta.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Delta") {
  auto delta = Delta<std::uint64_t, double>("delta");

  SECTION("emits difference") {
    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = delta.receive(Message<std::uint64_t, double>(i, i));
      if (i >= 2) {
        REQUIRE(emitted.find("delta")->second.at(0).value == -1);
        REQUIRE(emitted.find("delta")->second.at(0).time == i);
      }
    }
  }
}
