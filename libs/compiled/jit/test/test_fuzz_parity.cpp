// test_fuzz_parity.cpp
//
// Random-fuzz parity tester for the rtbot JIT. Generates valid Program JSON
// pipelines from a seeded RNG and pins per-emission bit-exact equality
// between the JIT and the framework operator-graph mode.
//
// Run with a specific seed via env var: RTBOT_FUZZ_SEED=42. The fuzz config
// (num_pipelines, ticks_per_test) can be overridden via env vars as well so
// developers can dial up coverage when chasing a flaky shape.

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "fuzz_helper.h"

namespace {

std::uint64_t env_uint64(const char* var, std::uint64_t fallback) {
  const char* s = std::getenv(var);
  if (!s || !*s) return fallback;
  try {
    return static_cast<std::uint64_t>(std::stoull(s));
  } catch (...) {
    return fallback;
  }
}

int env_int(const char* var, int fallback) {
  const char* s = std::getenv(var);
  if (!s || !*s) return fallback;
  try {
    return std::stoi(s);
  } catch (...) {
    return fallback;
  }
}

}  // namespace

SCENARIO("fuzz parity: random JIT-compatible pipelines vs framework",
         "[program][jit][framework_parity][fuzz]") {
  GIVEN("a seeded fuzz config") {
    rtbot::test::fuzz::FuzzConfig cfg;
    cfg.seed            = env_uint64("RTBOT_FUZZ_SEED", 42);
    cfg.num_pipelines   = env_int("RTBOT_FUZZ_NUM_PIPELINES", 200);
    cfg.ticks_per_test  = env_int("RTBOT_FUZZ_TICKS", 200);
    cfg.min_ops         = env_int("RTBOT_FUZZ_MIN_OPS", 3);
    cfg.max_ops         = env_int("RTBOT_FUZZ_MAX_OPS", 12);

    auto stats = rtbot::test::fuzz::run_fuzz(cfg);

    std::cout << "\n[fuzz] seed=" << cfg.seed
              << " generated=" << stats.generated
              << " jit_supported=" << (stats.generated - stats.skipped_jit_unsupported)
              << " tested=" << stats.tested
              << " passed=" << stats.passed
              << " failed=" << stats.failed
              << " skipped_invalid=" << stats.skipped_invalid
              << " skipped_jit_unsupported=" << stats.skipped_jit_unsupported
              << std::endl;

    if (!stats.failure_summaries.empty()) {
      std::cout << "[fuzz] divergences:\n";
      for (const auto& s : stats.failure_summaries) {
        std::cout << "  " << s << "\n";
      }
      std::cout.flush();
    }

    THEN("every JIT-compatible pipeline matches the framework bit-for-bit") {
      if (!stats.failure_summaries.empty()) {
        FAIL("fuzz parity divergence — see /tmp/fuzz_failure_" << cfg.seed
             << "_p*.json for reproductions; first summary: "
             << stats.failure_summaries.front());
      }
      REQUIRE(stats.failed == 0);
      REQUIRE(stats.tested > 0);
    }
  }
}

