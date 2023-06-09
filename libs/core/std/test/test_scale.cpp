#include <catch2/catch.hpp>

#include "rtbot/std/Scale.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Scale") {
  auto scale = Scale<std::uint64_t, double>("sc", 0.5);

  SECTION("emits scale 1/2") {
    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = scale.receive(Message<std::uint64_t, double>(i, i));
      REQUIRE(emitted.find("sc")->second.at(0).value == ((double)i / 2));
    }
  }
}
