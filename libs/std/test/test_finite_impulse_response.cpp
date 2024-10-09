#include <catch2/catch.hpp>

#include "rtbot/std/FiniteImpulseResponse.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Finite Impulse Response") {
  auto i1 = FiniteImpulseResponse<uint64_t, double>("i1", {2, 2, 2, 2, 2});
  auto i2 =
      FiniteImpulseResponse<uint64_t, double>("i2", {0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25});

  SECTION("emits linear combination") {
    for (int i = 0; i < 20; i++) {
      i1.receiveData(Message<uint64_t, double>(i * 100, 10));
      OperatorPayload<uint64_t, double> emitted = i1.executeData();

      if (i <= 3) {
        REQUIRE(emitted.empty());
      } else
        REQUIRE(emitted.find("i1")->second.find("o1")->second.at(0).value == 100);
    }
  }

  SECTION("emits correct average") {
    for (int i = 1; i < 20; i++) {
      i2.receiveData(Message<uint64_t, double>(i * 100, 20));
      OperatorPayload<uint64_t, double> emitted = i2.executeData();

      if (i <= 10) {
        REQUIRE(emitted.empty());
      } else {
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(0).value == 55);
      }
    }
  }
}