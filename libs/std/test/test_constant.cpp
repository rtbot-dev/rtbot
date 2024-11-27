#include <catch2/catch.hpp>

#include "rtbot/std/Constant.h"

using namespace rtbot;

SCENARIO("Constant operator handles basic operations", "[constant]") {
  GIVEN("A constant number operator") {
    auto constant = make_number_constant("const1", 42.0);

    WHEN("Single message is received") {
      constant->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      constant->execute();

      THEN("Message is emitted with constant value") {
        const auto& output = constant->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 42.0);
      }
    }

    WHEN("Multiple messages are received") {
      constant->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      constant->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
      constant->receive_data(create_message<NumberData>(4, NumberData{40.0}), 0);
      constant->execute();

      THEN("All messages are emitted with constant value") {
        const auto& output = constant->get_output_queue(0);
        REQUIRE(output.size() == 3);

        std::vector<timestamp_t> expected_times = {1, 2, 4};
        auto it = output.begin();
        for (size_t i = 0; i < expected_times.size(); ++i) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(it->get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == expected_times[i]);
          REQUIRE(msg->data.value == 42.0);
          ++it;
        }
      }
    }
  }

  GIVEN("A constant boolean operator") {
    auto constant = make_boolean_constant("const2", true);

    WHEN("Message is received") {
      constant->receive_data(create_message<BooleanData>(1, BooleanData{false}), 0);
      constant->execute();

      THEN("Message is emitted with constant boolean value") {
        const auto& output = constant->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<BooleanData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->data.value == true);
      }
    }
  }
}

SCENARIO("Constant operator handles error cases", "[constant]") {
  GIVEN("A constant operator") {
    auto constant = make_number_constant("const1", 42.0);

    WHEN("Wrong message type is received") {
      THEN("Type mismatch is detected") {
        REQUIRE_THROWS_AS(constant->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0),
                          std::runtime_error);
      }
    }

    WHEN("Invalid port index is used") {
      THEN("Error is thrown") {
        REQUIRE_THROWS_AS(constant->receive_data(create_message<NumberData>(1, NumberData{42.0}), 1),
                          std::runtime_error);
      }
    }
  }
}