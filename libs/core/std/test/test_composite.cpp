#include <catch2/catch.hpp>
#include <iostream>
#include <memory>

#include "rtbot/std/Composite.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PartialSum.h"

using namespace rtbot;
using namespace std;

/*TEST_CASE("Composite") {
  map<string, std::vector<Message<std::uint64_t, double>>> emitted;
  std::vector<Op_ptr<std::uint64_t, double>> x;
  x.push_back(std::make_unique<MovingAverage<std::uint64_t, double>>("ma", 10));
  x.push_back(std::make_unique<PartialSum<std::uint64_t, double>>("ps"));
  auto cmp = Composite<std::uint64_t, double>("cmp", std::move(x));

  SECTION("emits count") {
    map<string, std::vector<Message<std::uint64_t, double>>> emitted;
    for (int i = 1; i <= 20; i++) {
      emitted = cmp.receive(Message<std::uint64_t, double>(i, i));

      if (i < 10) {
        REQUIRE(emitted.empty());
      } else {
        REQUIRE(emitted.find("cmp")->second.at(0).value == 5.5 * (i - 9) + (i - 9) * (i - 10) / 2);
        REQUIRE(emitted.find("cmp")->second.size() == 1);
      }
    }
  }
}*/
