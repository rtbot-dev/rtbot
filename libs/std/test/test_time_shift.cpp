#include <catch2/catch.hpp>

#include "rtbot/std/TimeShift.h"

using namespace rtbot;

SCENARIO("TimeShift handles basic time shifting operations", "[TimeShift]") {
  GIVEN("A TimeShift operator with positive shift") {
    auto time_shift = std::make_unique<TimeShift>("shift1", 5);

    WHEN("Receiving messages") {
      time_shift->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      time_shift->execute();

      THEN("Messages are shifted forward in time") {
        const auto& output = time_shift->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 6);  // 1 + 5
        REQUIRE(msg->data.value == 42.0);
      }
    }
  }

  GIVEN("A TimeShift operator with negative shift") {
    auto time_shift = std::make_unique<TimeShift>("shift1", -2);

    WHEN("Processing messages that would result in negative time") {
      time_shift->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      // Will throw an exception "Negative new time" ...
      REQUIRE_THROWS(time_shift->execute());
    }

    WHEN("Processing messages that remain non-negative after shift") {
      time_shift->receive_data(create_message<NumberData>(5, NumberData{42.0}), 0);
      time_shift->execute();

      THEN("Messages are shifted backward in time") {
        const auto& output = time_shift->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);  // 5 - 2
        REQUIRE(msg->data.value == 42.0);
      }
    }
  }
}

SCENARIO("TimeShift handles multiple messages", "[TimeShift]") {
  GIVEN("A TimeShift operator with shift of 10") {
    auto time_shift = std::make_unique<TimeShift>("shift1", 10);

    WHEN("Processing multiple messages") {
      time_shift->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      time_shift->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      time_shift->receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
      time_shift->execute();

      THEN("All messages are shifted by the same amount") {
        const auto& output = time_shift->get_output_queue(0);
        REQUIRE(output.size() == 3);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1->time == 11);  // 1 + 10
        REQUIRE(msg1->data.value == 1.0);

        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2->time == 12);  // 2 + 10
        REQUIRE(msg2->data.value == 2.0);

        const auto* msg3 = dynamic_cast<const Message<NumberData>*>(output[2].get());
        REQUIRE(msg3->time == 14);  // 4 + 10
        REQUIRE(msg3->data.value == 4.0);
      }
    }
  }
}

SCENARIO("TimeShift handles state serialization", "[TimeShift]") {
  GIVEN("A TimeShift operator with processed messages") {
    auto time_shift = std::make_unique<TimeShift>("shift1", 5);

    // Fill input queue with messages
    time_shift->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    time_shift->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    time_shift->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = time_shift->collect();

      // Create new operator and restore state
      auto restored = std::make_unique<TimeShift>("shift1", 5);
      Bytes::const_iterator it = state.begin();
      restored->restore(it);

      // Execute both operators
      time_shift->execute();
      restored->execute();

      THEN("Restored operator produces identical output") {
        const auto& orig_output = time_shift->get_output_queue(0);
        const auto& rest_output = restored->get_output_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());
        REQUIRE(*time_shift == *restored);
        for (size_t i = 0; i < orig_output.size(); ++i) {
          const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[i].get());
          const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[i].get());

          REQUIRE(orig_msg->time == rest_msg->time);
          REQUIRE(orig_msg->data.value == rest_msg->data.value);
        }
      }
    }
  }
}