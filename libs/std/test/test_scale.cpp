#include <catch2/catch.hpp>

#include "rtbot/std/Scale.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Scale") {
  auto scale = Scale<uint64_t, double>("sc", 0.5);

  SECTION("emits scale 1/2") {
    OperatorPayload<uint64_t, double> emitted;
    for (int i = 1; i <= 50; i++) {
      scale.receiveData(Message<uint64_t, double>(i, i));
      emitted = scale.executeData();
      REQUIRE(emitted.find("sc")->second.find("o1")->second.at(0).value == ((double)i / 2));
    }
  }
}
