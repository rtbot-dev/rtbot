
#include <math.h>

#include <catch2/catch.hpp>

#include "rtbot/std/StandardDeviation.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Standard deviation") {
  auto i1 = StandardDeviation<uint64_t, double>("sd1", 5);
  auto i2 = StandardDeviation<uint64_t, double>("sd2", 10);

  SECTION("emits zeros") {
    for (int i = 0; i < 20; i++) {
      i1.receiveData(Message<uint64_t, double>(i * 100, 10));
      map<string, map<string, vector<Message<uint64_t, double>>>> emitted = i1.executeData();
      if (i <= 3) {
        REQUIRE(emitted.empty());
      } else
        REQUIRE(emitted.find("sd1")->second.find("o1")->second.at(0).value == 0);
    }
  }

  SECTION("emits correct std") {
    for (int i = 0; i < 10; i++) {
      i2.receiveData(Message<uint64_t, double>(i * 100, i + 1));
      map<string, map<string, vector<Message<uint64_t, double>>>> emitted = i2.executeData();

      if (i <= 8) {
        REQUIRE(emitted.empty());
      } else
        REQUIRE(((int)emitted.find("sd2")->second.find("o1")->second.at(0).value) == 3);
    }
  }
}