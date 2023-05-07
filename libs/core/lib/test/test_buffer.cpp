#include "rtbot/Message.h"
#include "rtbot/Output.h"
#include "rtbot/std/MovingAverage.h"
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <map>

using namespace rtbot;
using namespace std;

TEST_CASE("Buffer") {
  int dim = 3;
  string id = "buffer";
  auto buffer = MovingAverage<std::uint64_t, double>(id, dim);

  SECTION("Buffer constructor") {
    REQUIRE(buffer.n == dim);
    REQUIRE(buffer.id == id);
  }

  SECTION("received 10 messages check dim size invariant") {
    for (int i = 1; i <= 10; i++) {
      std::map<string, std::vector<rtbot::Message<std::uint64_t, double>>> emitted =
          buffer.receive(Message<std::uint64_t, double>(i, i));
      if (i < dim) {
        REQUIRE(emitted.empty());
      }

      if (i >= dim) {
        REQUIRE(buffer.at(0).value == i - 2);
        REQUIRE(buffer.at(1).value == i - 1);
        REQUIRE(buffer.at(2).value == i);
        REQUIRE(buffer.size() == dim);
        REQUIRE(buffer.getSum() == i + (i - 1) + (i - 2));
      }
    }
  }
}