#include <catch2/catch.hpp>

#include "rtbot/std/Difference.h"

using namespace rtbot;

SCENARIO("Difference operator handles basic operations", "[Difference]") {
  GIVEN("A Difference operator using newest time") {
    auto diff = std::make_unique<Difference>("diff1", false);

    WHEN("Single message is received") {
      diff->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      diff->execute();

      THEN("No output is produced") {
        const auto& output = diff->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }

    WHEN("Two messages are received") {
      diff->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      diff->execute();
      diff->receive_data(create_message<NumberData>(2, NumberData{15.0}), 0);
      diff->execute();

      THEN("Difference is calculated correctly") {
        const auto& output = diff->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 5.0);
      }
    }
  }

  GIVEN("A Difference operator using oldest time") {
    auto diff = std::make_unique<Difference>("diff2", true);

    WHEN("Processing a sequence of values") {
      diff->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      diff->receive_data(create_message<NumberData>(2, NumberData{15.0}), 0);
      diff->receive_data(create_message<NumberData>(3, NumberData{12.0}), 0);
      diff->execute();

      THEN("Time stamps are from newer messages") {
        const auto& output = diff->get_output_queue(0);
        REQUIRE(output.size() == 2);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1->time == 2);
        REQUIRE(msg1->data.value == 5.0);

        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2->time == 3);
        REQUIRE(msg2->data.value == -3.0);
      }
    }
  }
}

SCENARIO("Difference operator handles state serialization", "[Difference][State]") {
  GIVEN("A Difference operator with processed messages") {
    auto diff = std::make_unique<Difference>("diff3", true);

    // Add some messages
    diff->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    diff->receive_data(create_message<NumberData>(2, NumberData{15.0}), 0);
    diff->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = diff->collect();

      // Create new operator
      auto restored = std::make_unique<Difference>("diff3", true);

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Buffer content matches") {
        REQUIRE(restored->buffer_size() == diff->buffer_size());
        REQUIRE(restored->get_use_oldest_time() == diff->get_use_oldest_time());
      }

      AND_WHEN("New message is received") {
        restored->receive_data(create_message<NumberData>(3, NumberData{12.0}), 0);
        restored->execute();

        THEN("Calculation continues correctly") {
          const auto& output = restored->get_output_queue(0);
          REQUIRE(output.size() == 1);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
          REQUIRE(msg->time == 3);
          REQUIRE(msg->data.value == -3.0);
        }
      }
    }
  }
}