#include <catch2/catch.hpp>

#include <array>
#include <cstdint>

#include "rtbot/compiled/JoinStage.h"

using namespace rtbot::compiled;

SCENARIO("JoinStage<2> emits when timestamps align", "[join]") {
  JoinStage<2> join;
  std::int64_t t = 0;
  std::array<double, 2> vs{};

  // Push port 0 ahead at t=10, t=20.
  REQUIRE_FALSE(join.push(0, 10, 1.0, t, vs));
  REQUIRE_FALSE(join.push(0, 20, 2.0, t, vs));

  // Now push port 1 at t=20 — should sync, dropping port 0's t=10.
  REQUIRE(join.push(1, 20, 99.0, t, vs));
  REQUIRE(t == 20);
  REQUIRE(vs[0] == 2.0);
  REQUIRE(vs[1] == 99.0);

  // Pushing port 1 at t=30 alone does not sync (no port 0 yet at 30).
  REQUIRE_FALSE(join.push(1, 30, 100.0, t, vs));

  // Port 0 at t=30 → syncs.
  REQUIRE(join.push(0, 30, 3.0, t, vs));
  REQUIRE(t == 30);
  REQUIRE(vs[0] == 3.0);
  REQUIRE(vs[1] == 100.0);
}

SCENARIO("JoinStage<2> drops older mismatched fronts", "[join]") {
  JoinStage<2> join;
  std::int64_t t = 0;
  std::array<double, 2> vs{};

  // Port 0: 10, 15, 20.   Port 1: 20.
  // The Join should drop port 0's 10 and 15 and emit at t=20.
  REQUIRE_FALSE(join.push(0, 10, 1.0, t, vs));
  REQUIRE_FALSE(join.push(0, 15, 1.5, t, vs));
  REQUIRE_FALSE(join.push(0, 20, 2.0, t, vs));
  REQUIRE(join.push(1, 20, 22.0, t, vs));
  REQUIRE(t == 20);
  REQUIRE(vs[0] == 2.0);
  REQUIRE(vs[1] == 22.0);
}
