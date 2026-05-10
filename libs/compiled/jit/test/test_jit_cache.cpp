#include <catch2/catch.hpp>

#include <cstdint>

#include "rtbot/compiled/jit/JitCache.h"

namespace {

// Same JSON strings used by the existing e2e tests.
const char* kBollingerJson = R"({
  "title": "Bollinger Bands",
  "apiVersion": "v1",
  "entryOperator": "754",
  "output": { "37": ["o2", "o1", "o3"] },
  "operators": [
    { "id": "37", "type": "Output", "portTypes": ["number", "number", "number"] },
    { "id": "495", "type": "Subtraction" },
    { "id": "861", "type": "Addition" },
    { "id": "996", "type": "Scale", "value": 2 },
    { "id": "865", "type": "StandardDeviation", "window_size": 14 },
    { "id": "510", "type": "MovingAverage", "window_size": 14 },
    { "id": "262", "type": "ResamplerHermite", "interval": 1 },
    { "id": "754", "type": "Input", "portTypes": ["number"] }
  ],
  "connections": [
    { "from": "510", "to": "37", "fromPort": "o1", "toPort": "i3" },
    { "from": "495", "to": "37", "fromPort": "o1", "toPort": "i1" },
    { "from": "861", "to": "37", "fromPort": "o1", "toPort": "i2" },
    { "from": "996", "to": "495", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "495", "fromPort": "o1", "toPort": "i1" },
    { "from": "996", "to": "861", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "861", "fromPort": "o1", "toPort": "i1" },
    { "from": "865", "to": "996", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "865", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "510", "fromPort": "o1", "toPort": "i1" },
    { "from": "754", "to": "262", "fromPort": "o1", "toPort": "i1" }
  ]
})";

const char* kPpgJson = R"({
  "title": "PPG Peak Detection",
  "apiVersion": "v1",
  "entryOperator": "input",
  "output": { "out": ["o1", "o2"] },
  "operators": [
    { "id": "input",    "type": "Input",         "portTypes": ["number"] },
    { "id": "ma_short", "type": "MovingAverage", "window_size": 5 },
    { "id": "ma_long",  "type": "MovingAverage", "window_size": 30 },
    { "id": "join_ma",  "type": "Join",          "portTypes": ["number", "number"] },
    { "id": "minus",    "type": "Subtraction" },
    { "id": "peak",     "type": "PeakDetector",  "window_size": 11 },
    { "id": "join_out", "type": "Join",          "portTypes": ["number", "number"] },
    { "id": "out",      "type": "Output",        "portTypes": ["number", "number"] }
  ],
  "connections": [
    { "from": "input",    "to": "ma_short",  "fromPort": "o1", "toPort": "i1" },
    { "from": "input",    "to": "ma_long",   "fromPort": "o1", "toPort": "i1" },
    { "from": "ma_short", "to": "join_ma",   "fromPort": "o1", "toPort": "i1" },
    { "from": "ma_long",  "to": "join_ma",   "fromPort": "o1", "toPort": "i2" },
    { "from": "join_ma",  "to": "minus",     "fromPort": "o1", "toPort": "i1" },
    { "from": "join_ma",  "to": "minus",     "fromPort": "o2", "toPort": "i2" },
    { "from": "minus",    "to": "peak",      "fromPort": "o1", "toPort": "i1" },
    { "from": "peak",     "to": "join_out",  "fromPort": "o1", "toPort": "i1" },
    { "from": "input",    "to": "join_out",  "fromPort": "o1", "toPort": "i2" },
    { "from": "join_out", "to": "out",       "fromPort": "o1", "toPort": "i1" },
    { "from": "join_out", "to": "out",       "fromPort": "o2", "toPort": "i2" }
  ]
})";

}  // namespace

SCENARIO("JitCache get_or_compile caches by JSON shape", "[jit_cache]") {
  rtbot::jit::JitCache cache;
  REQUIRE(cache.size() == 0);

  auto p1 = cache.get_or_compile(kBollingerJson);
  REQUIRE(p1 != nullptr);
  REQUIRE(cache.size() == 1);

  auto p2 = cache.get_or_compile(kBollingerJson);  // same JSON -> cache hit
  REQUIRE(p2 != nullptr);
  REQUIRE(cache.size() == 1);  // still only one compiled entry

  // Drive both programs with the same inputs and expect identical outputs.
  for (int i = 0; i < 100; ++i) {
    p1->receive(static_cast<std::int64_t>(i), static_cast<double>(i) + 1.0);
    p2->receive(static_cast<std::int64_t>(i), static_cast<double>(i) + 1.0);
  }
  auto out1 = p1->collect_outputs();
  auto out2 = p2->collect_outputs();
  REQUIRE(out1.size() == out2.size());
  for (std::size_t i = 0; i < out1.size(); ++i) {
    INFO("record i=" << i);
    REQUIRE(out1[i].time == out2[i].time);
    REQUIRE(out1[i].values.size() == out2[i].values.size());
    for (std::size_t j = 0; j < out1[i].values.size(); ++j) {
      REQUIRE(out1[i].values[j] == out2[i].values[j]);
    }
  }
}

SCENARIO("JitCache distinguishes different JSON shapes", "[jit_cache]") {
  rtbot::jit::JitCache cache;
  cache.get_or_compile(kBollingerJson);
  cache.get_or_compile(kPpgJson);
  REQUIRE(cache.size() == 2);

  // Requesting either again must not grow the cache.
  cache.get_or_compile(kBollingerJson);
  REQUIRE(cache.size() == 2);
}

SCENARIO("JitCache::clear empties the cache", "[jit_cache]") {
  rtbot::jit::JitCache cache;
  cache.get_or_compile(kBollingerJson);
  REQUIRE(cache.size() == 1);
  cache.clear();
  REQUIRE(cache.size() == 0);

  // Recompile after clear works.
  auto prog = cache.get_or_compile(kBollingerJson);
  REQUIRE(prog != nullptr);
  REQUIRE(cache.size() == 1);
}

SCENARIO("JitCache singleton instance survives across calls", "[jit_cache]") {
  auto& c1 = rtbot::jit::JitCache::instance();
  c1.clear();

  c1.get_or_compile(kBollingerJson);
  REQUIRE(rtbot::jit::JitCache::instance().size() == 1);

  // A second reference to the singleton should see the same entry.
  auto& c2 = rtbot::jit::JitCache::instance();
  REQUIRE(&c1 == &c2);
  REQUIRE(c2.size() == 1);

  c1.clear();
  REQUIRE(c2.size() == 0);
}

SCENARIO("JitCache instances are state-independent", "[jit_cache]") {
  // Two programs from the same cache entry must not share state: advancing
  // p1 by N steps must not affect p2's outputs.
  rtbot::jit::JitCache cache;

  auto p1 = cache.get_or_compile(kBollingerJson);
  auto p2 = cache.get_or_compile(kBollingerJson);

  // Feed p1 a warm-up run of 50 samples so it accumulates internal state.
  for (int i = 0; i < 50; ++i) {
    p1->receive(static_cast<std::int64_t>(i), static_cast<double>(i) + 1.0);
  }
  p1->collect_outputs();  // drain

  // p2 starts fresh: drive identical samples through both, reset first 50 for p2.
  for (int i = 0; i < 50; ++i) {
    p2->receive(static_cast<std::int64_t>(i), static_cast<double>(i) + 1.0);
  }
  auto out2_warmup = p2->collect_outputs();

  // Continue both from sample 50 with identical data.
  for (int i = 50; i < 100; ++i) {
    double v = static_cast<double>(i) + 1.0;
    p1->receive(static_cast<std::int64_t>(i), v);
    p2->receive(static_cast<std::int64_t>(i), v);
  }
  auto out1_tail = p1->collect_outputs();
  auto out2_tail = p2->collect_outputs();

  // Both programs ran the same 50 samples from position 50; outputs must match.
  REQUIRE(out1_tail.size() == out2_tail.size());
  for (std::size_t i = 0; i < out1_tail.size(); ++i) {
    REQUIRE(out1_tail[i].time == out2_tail[i].time);
    for (std::size_t j = 0; j < out1_tail[i].values.size(); ++j) {
      REQUIRE(out1_tail[i].values[j] == out2_tail[i].values[j]);
    }
  }
}
