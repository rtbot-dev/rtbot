#include <catch2/catch.hpp>

#include "rtbot/std/Identity.h"

using namespace rtbot;

SCENARIO("Identity operator handles basic message forwarding", "[identity]") {
  GIVEN("An identity operator") {
    auto identity = make_identity("id1");

    WHEN("Receiving a single message") {
      identity->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      identity->execute();

      THEN("Message is forwarded unchanged") {
        const auto& output = identity->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 42.0);
      }
    }

    WHEN("Receiving multiple messages") {
      identity->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      identity->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      identity->receive_data(create_message<NumberData>(5, NumberData{5.0}), 0);
      identity->execute();

      THEN("All messages are forwarded in order") {
        const auto& output = identity->get_output_queue(0);
        REQUIRE(output.size() == 3);

        auto it = output.begin();
        std::vector<std::pair<timestamp_t, double>> expected = {{1, 1.0}, {3, 3.0}, {5, 5.0}};

        for (const auto& [exp_time, exp_value] : expected) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>((*it).get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == exp_time);
          REQUIRE(msg->data.value == exp_value);
          ++it;
        }
      }
    }
  }
}

SCENARIO("Identity operator handles state serialization", "[identity]") {
  GIVEN("An identity operator with processed messages") {
    auto identity = make_identity("id1");

    // Add some messages
    identity->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    identity->receive_data(create_message<NumberData>(3, NumberData{30.0}), 0);
    identity->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = identity->collect();

      // Create new operator
      auto restored = make_identity("id1");

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Behavior is preserved") {
        restored->clear_all_output_ports();
        restored->receive_data(create_message<NumberData>(5, NumberData{50.0}), 0);
        restored->execute();

        const auto& output = restored->get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 5);
        REQUIRE(msg->data.value == 50.0);
      }
    }
  }
}

SCENARIO("Identity operator validates message types", "[identity]") {
  GIVEN("An identity operator") {
    auto identity = make_identity("id1");

    THEN("It rejects invalid message types") {
      REQUIRE_THROWS_AS(identity->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0),
                        std::runtime_error);
    }
  }
}