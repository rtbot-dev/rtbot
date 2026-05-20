#include <catch2/catch.hpp>

#include "rtbot/compiled/ScaleStage.h"

using namespace rtbot::compiled;

SCENARIO("ScaleStage multiplies input by stored constant", "[scale]") {
  ScaleStage scale_2{2.0};
  REQUIRE(scale_2.process(3.5) == 7.0);
  REQUIRE(scale_2.process(-1.0) == -2.0);
  REQUIRE(scale_2.process(0.0) == 0.0);

  ScaleStage scale_neg{-1.5};
  REQUIRE(scale_neg.process(2.0) == -3.0);
}
