#include <catch2/catch.hpp>

#include "rtbot/std/GreaterThan.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Greater Than") {
  auto gt = GreaterThan<uint64_t, double>("gt", 0.4);

  SECTION("only one emission") {
    int t = 0;
    int sign = 1;
    double v = 0.0;

    OperatorPayload<uint64_t, double> emitted;
    for (int i = 1; i <= 11; i++) {
      t++;
      v += sign * 0.1;
      if (t % 6 == 0) sign = -sign;
      gt.receiveData(Message<uint64_t, double>(i, v));
      emitted = gt.executeData();
      if (i < 5) {
        REQUIRE(emitted.empty());
      } else if (i == 5 || i == 6 || i == 7) {
        REQUIRE(emitted.find("gt")->second.find("o1")->second.at(0).value == v);
        REQUIRE(emitted.find("gt")->second.find("o1")->second.at(0).time == i);
        REQUIRE(emitted.find("gt")->second.size() == 1);
      } else if (i > 7) {
        REQUIRE(emitted.empty());
      }
    }
  }
}
