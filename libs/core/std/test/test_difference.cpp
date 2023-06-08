#include <catch2/catch.hpp>

#include "rtbot/std/Difference.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Difference") {
  map<string, std::vector<Message<std::uint64_t, double>>> emitted;
  auto diff = Difference<std::uint64_t, double>("diff");

  SECTION("emits difference") {
    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = diff.receive(Message<std::uint64_t, double>(i, i));
      if (i >= 2) {
        REQUIRE(emitted.find("diff")->second.at(0).value == -1);
        REQUIRE(emitted.find("diff")->second.at(0).time == i);
      }
    }
  }
}
