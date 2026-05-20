#include <catch2/catch.hpp>

#include "rtbot/compiled/ArithmeticStage.h"

using namespace rtbot::compiled;

SCENARIO("AdditionStage adds two doubles", "[arithmetic]") {
  AdditionStage add;
  REQUIRE(add.process(1.5, 2.25) == 3.75);
  REQUIRE(add.process(-1.0, 1.0) == 0.0);
  REQUIRE(add.process(0.0, 0.0) == 0.0);
}

SCENARIO("SubtractionStage subtracts the second from the first", "[arithmetic]") {
  SubtractionStage sub;
  REQUIRE(sub.process(5.0, 2.0) == 3.0);
  REQUIRE(sub.process(0.0, 1.0) == -1.0);
  REQUIRE(sub.process(1.5, 1.5) == 0.0);
}

SCENARIO("MultiplicationStage and DivisionStage compute the obvious", "[arithmetic]") {
  MultiplicationStage mul;
  DivisionStage div;
  REQUIRE(mul.process(3.0, 4.0) == 12.0);
  REQUIRE(div.process(10.0, 4.0) == 2.5);
}
