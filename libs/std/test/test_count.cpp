#include <catch2/catch.hpp>

#include "rtbot/std/Count.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Count") {
  auto c = Count<uint64_t, double>("c");

  SECTION("emits count") {
    OperatorPayload<uint64_t, double> emitted;
    for (int i = 1; i <= 50; i++) {
      c.receiveData(Message<uint64_t, double>(i, i));
      emitted = c.executeData();
      REQUIRE(emitted.find("c")->second.find("o1")->second.at(0).value == i);
    }
  }
}
