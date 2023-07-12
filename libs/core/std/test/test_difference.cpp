#include <catch2/catch.hpp>

#include "rtbot/std/Difference.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Difference") {
  auto diff = Difference<uint64_t, double>("diff");

  SECTION("emits difference") {
    map<string, map<string, vector<Message<uint64_t, double>>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = diff.receiveData(Message<uint64_t, double>(i, i));
      if (i >= 2) {
        REQUIRE(emitted.find("diff")->second.find("o1")->second.at(0).value == 1);
        REQUIRE(emitted.find("diff")->second.find("o1")->second.at(0).time == i);
      }
    }
  }
}
