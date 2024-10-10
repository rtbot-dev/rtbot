#include <catch2/catch.hpp>

#include "rtbot/std/Add.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Add") {
  auto add = Add<uint64_t, double>("add", 1);

  SECTION("emits add 1") {
    ProgramMessage<uint64_t, double> emitted;
    for (int i = 1; i <= 50; i++) {
      add.receiveData(Message<uint64_t, double>(i, i));
      emitted = add.executeData();
      REQUIRE(emitted.find("add")->second.find("o1")->second.at(0).value == i + 1);
      REQUIRE(emitted.find("add")->second.find("o1")->second.at(0).time == i);
    }
  }
}
