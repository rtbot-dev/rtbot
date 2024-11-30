#include <catch2/catch.hpp>

#include "rtbot/std/Count.h"

using namespace rtbot;

SCENARIO("Count operator handles basic counting", "[Count]") {
  GIVEN("A Count operator") {
    auto counter = std::make_unique<Count<NumberData>>("counter");

    WHEN("Receiving messages of different types") {
      counter->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      counter->receive_data(create_message<NumberData>(2, NumberData{37}), 0);
      counter->receive_data(create_message<NumberData>(3, NumberData{-2.2}), 0);
      counter->execute();

      THEN("Counts messages regardless of type") {
        const auto& output = counter->get_output_queue(0);
        REQUIRE(output.size() == 3);

        for (size_t i = 0; i < output.size(); i++) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
          REQUIRE(msg->time == i + 1);
          REQUIRE(msg->data.value == i + 1);
        }
      }
    }
  }
}

SCENARIO("Count operator handles state serialization", "[Count][State]") {
  GIVEN("A Count operator with some history") {
    auto counter = std::make_unique<Count<BooleanData>>("counter");

    // Process initial messages
    counter->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    counter->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    counter->execute();

    WHEN("State is serialized and restored") {
      Bytes state = counter->collect();
      auto restored = std::make_unique<Count<BooleanData>>("counter");
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Continues counting from previous state") {
        restored->clear_all_output_ports();
        restored->receive_data(create_message<BooleanData>(3, BooleanData{true}), 0);
        restored->execute();

        const auto& output = restored->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == 3.0);
      }
    }
  }
}