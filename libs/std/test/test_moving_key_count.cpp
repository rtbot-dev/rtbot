#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/std/MovingKeyCount.h"

using namespace rtbot;

SCENARIO("MovingKeyCount counts key occurrences in a sliding window", "[moving_key_count]") {
  SECTION("Basic window counting") {
    // window_size=5, sequence [1, 2, 1, 1, 2, 3, 1, 2, 1, 1]
    // From roadmap test case
    auto mkc = make_moving_key_count("mkc1", 5);
    REQUIRE(mkc->type_name() == "MovingKeyCount");
    REQUIRE(mkc->get_window_size() == 5);

    std::vector<double> keys = {1, 2, 1, 1, 2, 3, 1, 2, 1, 1};
    for (size_t i = 0; i < keys.size(); i++) {
      mkc->receive_data(
          create_message<NumberData>(static_cast<timestamp_t>(i + 1),
                                     NumberData{keys[i]}),
          0);
    }
    mkc->execute();

    auto& out = mkc->get_output_queue(0);
    REQUIRE(out.size() == 10);

    // At t=1: window=[1],          key=1 → count=1
    // At t=2: window=[1,2],        key=2 → count=1
    // At t=3: window=[1,2,1],      key=1 → count=2
    // At t=4: window=[1,2,1,1],    key=1 → count=3
    // At t=5: window=[1,2,1,1,2],  key=2 → count=2
    // At t=6: window=[2,1,1,2,3],  key=3 → count=1  (evicts t=1: key=1)
    // At t=7: window=[1,1,2,3,1],  key=1 → count=3  (evicts t=2: key=2)
    // At t=8: window=[1,2,3,1,2],  key=2 → count=2  (evicts t=3: key=1)
    // At t=9: window=[2,3,1,2,1],  key=1 → count=2  (evicts t=4: key=1)
    // At t=10:window=[3,1,2,1,1],  key=1 → count=3  (evicts t=5: key=2)
    std::vector<double> expected = {1, 1, 2, 3, 2, 1, 3, 2, 2, 3};
    for (size_t i = 0; i < expected.size(); i++) {
      const auto* msg =
          dynamic_cast<const Message<NumberData>*>(out[i].get());
      REQUIRE(msg->time == static_cast<timestamp_t>(i + 1));
      REQUIRE(msg->data.value == Approx(expected[i]));
    }
  }
}

SCENARIO("MovingKeyCount emits current key count, not window total", "[moving_key_count]") {
  SECTION("Same key every message") {
    auto mkc = make_moving_key_count("mkc1", 3);
    // Send key=42 four times — window fills then slides
    mkc->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
    mkc->receive_data(create_message<NumberData>(2, NumberData{42.0}), 0);
    mkc->receive_data(create_message<NumberData>(3, NumberData{42.0}), 0);
    mkc->receive_data(create_message<NumberData>(4, NumberData{42.0}), 0);
    mkc->execute();

    auto& out = mkc->get_output_queue(0);
    REQUIRE(out.size() == 4);
    // t=1: window=[42] → 1
    // t=2: window=[42,42] → 2
    // t=3: window=[42,42,42] → 3
    // t=4: window=[42,42,42] (evicts oldest 42) → 3
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(1.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[1].get())->data.value == Approx(2.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[2].get())->data.value == Approx(3.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[3].get())->data.value == Approx(3.0));
  }

  SECTION("Key drops to zero after eviction") {
    auto mkc = make_moving_key_count("mkc1", 2);
    // key=1 at t=1, then key=2 at t=2, t=3 — key=1 evicted after t=3
    mkc->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    mkc->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    mkc->receive_data(create_message<NumberData>(3, NumberData{2.0}), 0);
    mkc->receive_data(create_message<NumberData>(4, NumberData{1.0}), 0);
    mkc->execute();

    auto& out = mkc->get_output_queue(0);
    REQUIRE(out.size() == 4);
    // t=1: window=[1]   → key=1 count=1
    // t=2: window=[1,2] → key=2 count=1
    // t=3: window=[2,2] → key=2 count=2  (evicts t=1: key=1)
    // t=4: window=[2,1] → key=1 count=1  (evicts t=2: key=2)
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(1.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[1].get())->data.value == Approx(1.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[2].get())->data.value == Approx(2.0));
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[3].get())->data.value == Approx(1.0));
  }
}

SCENARIO("MovingKeyCount window_size=1 always returns 1", "[moving_key_count]") {
  SECTION("Every message is its own window") {
    auto mkc = make_moving_key_count("mkc1", 1);
    mkc->receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
    mkc->receive_data(create_message<NumberData>(2, NumberData{5.0}), 0);
    mkc->receive_data(create_message<NumberData>(3, NumberData{9.0}), 0);
    mkc->execute();

    auto& out = mkc->get_output_queue(0);
    REQUIRE(out.size() == 3);
    for (size_t i = 0; i < 3; i++) {
      REQUIRE(dynamic_cast<const Message<NumberData>*>(out[i].get())->data.value == Approx(1.0));
    }
  }
}

SCENARIO("MovingKeyCount serialization roundtrip", "[moving_key_count][State]") {
  SECTION("Collect and restore mid-stream") {
    auto mkc = make_moving_key_count("mkc1", 3);
    mkc->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    mkc->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    mkc->receive_data(create_message<NumberData>(3, NumberData{1.0}), 0);
    mkc->execute();

    auto state = mkc->collect();
    auto restored = make_moving_key_count("mkc1", 3);
    restored->restore_data_from_json(state);
    REQUIRE(*restored == *mkc);

    // Continue feeding — window=[1,2,1], now add key=1 → evicts oldest key=1 → count=2
    restored->clear_all_output_ports();
    restored->receive_data(create_message<NumberData>(4, NumberData{1.0}), 0);
    restored->execute();

    auto& out = restored->get_output_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(2.0));
  }
}

SCENARIO("MovingKeyCount rejects window_size=0", "[moving_key_count]") {
  SECTION("Constructor throws") {
    REQUIRE_THROWS_AS(make_moving_key_count("mkc1", 0), std::runtime_error);
  }
}
