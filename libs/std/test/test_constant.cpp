#include <catch2/catch.hpp>

#include "rtbot/std/Constant.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Constant") {
  auto constant = Constant<uint64_t, double>("const", 0.5);

  SECTION("emits constant 1/2 every time") {
    ProgramMessage<uint64_t, double> emitted;
    for (int i = 1; i <= 50; i++) {
      constant.receiveData(Message<uint64_t, double>(i, i));
      emitted = constant.executeData();
      REQUIRE(emitted.find("const")->second.find("o1")->second.at(0).value == 0.5);
    }
  }
}
