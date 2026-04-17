#include <catch2/catch.hpp>

#include "fused_parity/reference_eval.h"
#include "rtbot/fuse/FusedExpression.h"

using namespace rtbot::fused_parity;
using namespace rtbot::fused_op;

SCENARIO("reference_eval computes a + b", "[reference_eval]") {
  auto r = evaluate_scalar(
      /*bytecode*/ {INPUT, 0, INPUT, 1, ADD, END},
      /*constants*/ {},
      /*inputs_per_message*/ {{2.0, 3.0}},
      /*state_init*/ {},
      /*num_outputs*/ 1);
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0] == 5.0);
}

SCENARIO("reference_eval sequences stateful CUMSUM across messages", "[reference_eval][state]") {
  auto r = evaluate_scalar(
      /*bytecode*/ {INPUT, 0, CUMSUM, 0, END},
      /*constants*/ {},
      /*inputs_per_message*/ {{1.0}, {2.0}, {3.0}},
      /*state_init*/ {0.0, 0.0},
      /*num_outputs*/ 1);
  REQUIRE(r.outputs.size() == 3);
  REQUIRE(r.outputs[0] == 1.0);
  REQUIRE(r.outputs[1] == 3.0);
  REQUIRE(r.outputs[2] == 6.0);
}
