#include <math.h>

#include <catch2/catch.hpp>

#include "rtbot/std/AutoRegressive.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Auto Regressive") {
  auto i1 = AutoRegressive<uint64_t, double>("ar", {1, 1, 1, 1, 1, 1});

  SECTION("emits powers of 2") {
    for (int i = 1; i <= 6; i++) {
      i1.receiveData(Message<uint64_t, double>(i, 1));
      ProgramMessage<uint64_t, double> emitted = i1.executeData();
      REQUIRE(emitted.find("ar")->second.find("o1")->second.at(0).value == pow(2, i - 1));
    }
  }
}
