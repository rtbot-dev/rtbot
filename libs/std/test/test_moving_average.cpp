#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "rtbot/std/MovingAverage.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Moving  average") {
  auto i1 = MovingAverage<uint64_t, double>("i1", 5);
  auto i2 = MovingAverage<uint64_t, double>("i2", 11);

  SECTION("emits same") {
    for (int i = 0; i < 20; i++) {
      i1.receiveData(Message<uint64_t, double>(i * 100, 10));
      ProgramMessage<uint64_t, double> emitted = i1.executeData();

      if (i <= 3) {
        REQUIRE(emitted.empty());
      } else
        REQUIRE(emitted.find("i1")->second.find("o1")->second.at(0).value == 10);
    }
  }

  SECTION("emits correct average") {
    for (int i = 1; i < 20; i++) {
      i2.receiveData(Message<uint64_t, double>(i * 100, i));
      ProgramMessage<uint64_t, double> emitted = i2.executeData();

      if (i <= 10) {
        REQUIRE(emitted.empty());
      } else {
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(0).value == i - 5);
      }
    }
  }
}