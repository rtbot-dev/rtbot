#include <catch2/catch.hpp>

#include "rtbot/std/Add.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Add") {
  auto add = Add<uint64_t, double>("add", 1);

  SECTION("emits add 1") {
    map<string, vector<Message<uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = add.receive(Message<uint64_t, double>(i, i));
      REQUIRE(emitted.find("add")->second.at(0).value == i + 1);
      REQUIRE(emitted.find("add")->second.at(0).time == i);
    }
  }
}
