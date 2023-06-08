#include <catch2/catch.hpp>

#include "rtbot/std/Count.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Count") {
  map<string, std::vector<Message<std::uint64_t, double>>> emitted;
  auto c = Count<std::uint64_t, double>("c");

  SECTION("emits count") {
    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 50; i++) {
      emitted = c.receive(Message<std::uint64_t, double>(i, i));
      REQUIRE(emitted.find("c")->second.at(0).value == i);
    }
  }
}
