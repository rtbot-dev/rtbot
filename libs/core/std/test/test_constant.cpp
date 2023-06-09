#include <catch2/catch.hpp>

#include "rtbot/std/Constant.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Constant") {
  auto constant = Constant<std::uint64_t, double>("const", 0.5);

  SECTION("emits constant 1/2 every time") {
    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = constant.receive(Message<std::uint64_t, double>(i, i));
      REQUIRE(emitted.find("const")->second.at(0).value == 0.5);
    }
  }
}
