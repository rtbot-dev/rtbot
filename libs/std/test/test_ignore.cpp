#include <catch2/catch.hpp>

#include "rtbot/std/Ignore.h"

using namespace rtbot;

SCENARIO("Ignored operator handles basic message forwarding", "[ignore]") {
  GIVEN("An ignored operator") {
    auto ignored = make_ignore("id1", 0);

    WHEN("Receiving a single message") {
      ignored->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      ignored->execute();

      THEN("Message is forwarded unchanged") {
        const auto& output = ignored->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 42.0);
      }
    }

    WHEN("Receiving multiple messages") {
      ignored->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      ignored->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      ignored->receive_data(create_message<NumberData>(5, NumberData{5.0}), 0);
      ignored->execute();

      THEN("All messages are forwarded in order") {
        const auto& output = ignored->get_output_queue(0);
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

SCENARIO("Ignored operator handles basic message forwarding when ignoring 3 messages", "[ignore]") {
  GIVEN("An ignored operator") {
    auto ignored = make_ignore("id1", 3);

    WHEN("Receiving a single message") {
      ignored->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      ignored->execute();

      THEN("No message is found") {
        const auto& output = ignored->get_output_queue(0);
        REQUIRE(output.size() == 0);
      }
    }

    WHEN("Receiving 4 messages only 1 forwarded beacuse we are ignoring 3") {
      ignored->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      ignored->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      ignored->receive_data(create_message<NumberData>(5, NumberData{5.0}), 0);
      ignored->receive_data(create_message<NumberData>(6, NumberData{6.0}), 0);
      ignored->execute();

      THEN("Only 1 message is forwarded") {
        const auto& output = ignored->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 6);
        REQUIRE(msg->data.value == 6.0);
      }
    }

    WHEN(
        "Receiving 5 messages only 1 forwarded beacuse we are ignoring 3 and then receiving a control message to reset "
        "back to 0 ignored") {
      ignored->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      ignored->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      ignored->receive_data(create_message<NumberData>(5, NumberData{5.0}), 0);
      ignored->receive_data(create_message<NumberData>(6, NumberData{6.0}), 0);
      ignored->receive_data(create_message<NumberData>(7, NumberData{7.0}), 0);
      ignored->execute();

      THEN(
          "Only 2 message is forwarded, after that we received a control message and 4 data messages and only 1 is "
          "forwarded") {
        const auto& output = ignored->get_output_queue(0);
        REQUIRE(output.size() == 2);
        ignored->clear_all_output_ports();
        ignored->receive_control(create_message<NumberData>(9, NumberData{1}), 0);
        ignored->receive_data(create_message<NumberData>(9, NumberData{1.0}), 0);
        ignored->receive_data(create_message<NumberData>(10, NumberData{3.0}), 0);
        ignored->receive_data(create_message<NumberData>(11, NumberData{5.0}), 0);
        ignored->receive_data(create_message<NumberData>(12, NumberData{6.0}), 0);
        ignored->execute();
        const auto& output1 = ignored->get_output_queue(0);
        REQUIRE(output1.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output1.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 12);
        REQUIRE(msg->data.value == 6.0);
      }
    }
  }
}

SCENARIO("Ignore operator handles state serialization", "[ignore]") {
  GIVEN("An ignore operator with processed messages") {
    auto ignore = make_ignore("id1", 3);

    // Add some messages
    ignore->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    ignore->receive_data(create_message<NumberData>(3, NumberData{30.0}), 0);
    ignore->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = ignore->collect();

      // Create new operator
      auto restored = make_ignore("id1", 0);

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Behavior is preserved") {
        restored->clear_all_output_ports();
        restored->receive_data(create_message<NumberData>(5, NumberData{50.0}), 0);
        restored->receive_data(create_message<NumberData>(6, NumberData{60.0}), 0);
        restored->execute();

        const auto& output = restored->get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 6);
        REQUIRE(msg->data.value == 60.0);
      }
    }
  }
}

SCENARIO("Ignored operator validates message types", "[ignore]") {
  GIVEN("An ignored operator") {
    auto ignore = make_ignore("id1", 0);

    THEN("It rejects invalid message types") {
      REQUIRE_THROWS_AS(ignore->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
    }
  }
}