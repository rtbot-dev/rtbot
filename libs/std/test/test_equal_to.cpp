#include <catch2/catch.hpp>

#include "rtbot/std/EqualTo.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Equal To") {
  auto et0 = EqualTo<uint64_t, double>("et0", 0);
  auto et1 = EqualTo<uint64_t, double>("et1", 1);

  SECTION("only one emission") {
    ProgramMessage<uint64_t, double> emitted0;
    ProgramMessage<uint64_t, double> emitted1;
    for (int i = 1; i <= 20; i++) {
      et0.receiveData(Message<uint64_t, double>(i, i % 2));
      et1.receiveData(Message<uint64_t, double>(i, i % 2));
      emitted0 = et0.executeData();
      emitted1 = et1.executeData();
      if (i % 2 == 0) {
        REQUIRE(emitted0.find("et0")->second.find("o1")->second.at(0).value == 0);
        REQUIRE(emitted0.find("et0")->second.find("o1")->second.at(0).time == i);
        REQUIRE(emitted0.find("et0")->second.size() == 1);
        REQUIRE(emitted1.empty());
      } else {
        REQUIRE(emitted1.find("et1")->second.find("o1")->second.at(0).value == 1);
        REQUIRE(emitted1.find("et1")->second.find("o1")->second.at(0).time == i);
        REQUIRE(emitted1.find("et1")->second.size() == 1);
        REQUIRE(emitted0.empty());
      }
    }
  }
}
