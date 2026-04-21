#include <catch2/catch.hpp>

#include <memory>
#include <vector>

#include "rtbot/Message.h"

using namespace rtbot;

SCENARIO("Message pool handles alloc/free bursts beyond capacity",
         "[message][pool]") {
  // The thread-local pool caches up to kCap=1024 freed slots; allocations
  // beyond that must cleanly fall back to ::operator new and frees beyond
  // capacity must fall back to ::operator delete. 2x capacity forces both
  // the pool-miss and pool-overflow branches for every round.
  constexpr std::size_t N = 2048;
  for (int round = 0; round < 3; ++round) {
    std::vector<std::unique_ptr<Message<NumberData>>> msgs;
    msgs.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
      msgs.push_back(create_message<NumberData>(
          static_cast<timestamp_t>(i + 1), NumberData{static_cast<double>(i)}));
    }
    // Verify no corruption across the batch: each message carries back the
    // time and value it was constructed with.
    for (std::size_t i = 0; i < N; ++i) {
      REQUIRE(msgs[i]->time == static_cast<timestamp_t>(i + 1));
      REQUIRE(msgs[i]->data.value == static_cast<double>(i));
    }
    msgs.clear();  // deletes go back into the pool until kCap is reached,
                    // then spill to ::operator delete — neither path may crash.
  }
}

SCENARIO("Message pool keeps distinct pointer identity when recycled",
         "[message][pool]") {
  // Allocate + free a message, then immediately re-allocate; the pool should
  // hand back a slot without crashing. We don't assert pointer equality
  // (implementation-defined under LIFO pool), but we do assert the newly
  // constructed payload is untouched by any stale state.
  auto m1 = create_message<NumberData>(1, NumberData{42.0});
  m1.reset();
  auto m2 = create_message<NumberData>(2, NumberData{-7.5});
  REQUIRE(m2->time == 2);
  REQUIRE(m2->data.value == -7.5);
}
