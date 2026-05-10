#include <catch2/catch.hpp>

#include "rtbot/compiled/jit/JitCompiler.h"

SCENARIO("probe_runtime_support succeeds in normal environment", "[jit_probe]") {
  REQUIRE(rtbot::jit::probe_runtime_support());
  // Second call returns cached result without re-probing.
  REQUIRE(rtbot::jit::probe_runtime_support());
}
