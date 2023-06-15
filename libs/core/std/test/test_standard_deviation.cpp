
#include <math.h>

#include <catch2/catch.hpp>

#include "rtbot/std/StandardDeviation.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Standard deviation") {
  auto i1 = StandardDeviation<uint64_t, double>("i1", 5);
  auto i2 = StandardDeviation<uint64_t, double>("i2", 10);

  SECTION("emits zeros") {
    for (int i = 0; i < 20; i++) {
      map<string, vector<Message<uint64_t, double>>> emitted = i1.receiveData(Message<uint64_t, double>(i * 100, 10));
      if (i <= 3) {
        REQUIRE(emitted.empty());
      } else
        REQUIRE(emitted.find("i1")->second.at(0).value == 0);
    }
  }

  SECTION("emits correct std") {
    for (int i = 0; i < 10; i++) {
      map<string, vector<Message<uint64_t, double>>> emitted =
          i2.receiveData(Message<uint64_t, double>(i * 100, i + 1));
      if (i <= 8) {
        REQUIRE(emitted.empty());
      } else
        REQUIRE(((int)emitted.find("i2")->second.at(0).value) == 3);
    }
  }
}