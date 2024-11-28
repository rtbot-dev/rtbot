#include <catch2/catch.hpp>

#include "rtbot/std/Division.h"

using namespace rtbot;

SCENARIO("Division operator handles basic division", "[division]") {
  GIVEN("A division operator") {
    auto div = std::make_unique<Division>("div1");

    WHEN("Receiving synchronized messages with valid division") {
      div->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      div->receive_data(create_message<NumberData>(1, NumberData{2.0}), 1);
      div->execute();

      THEN("Output is correct division") {
        const auto& output = div->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 5.0);
      }
    }

    WHEN("Receiving division by zero") {
      div->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      div->receive_data(create_message<NumberData>(1, NumberData{0.0}), 1);
      div->execute();

      THEN("No output is produced") {
        const auto& output = div->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }

    WHEN("Receiving messages at different times") {
      div->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      div->receive_data(create_message<NumberData>(2, NumberData{2.0}), 1);
      div->execute();

      THEN("No output is produced") {
        const auto& output = div->get_output_queue(0);
        REQUIRE(output.empty());
      }

      AND_WHEN("Matching timestamp arrives") {
        div->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
        div->execute();

        THEN("Division is computed") {
          const auto& output = div->get_output_queue(0);
          REQUIRE(output.size() == 1);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
          REQUIRE(msg->time == 2);
          REQUIRE(msg->data.value == 10.0);
        }
      }
    }
  }
}

SCENARIO("Division operator handles state serialization", "[division][serialization]") {
  GIVEN("A division operator with pending messages") {
    auto div = std::make_unique<Division>("div1");

    div->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    div->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
    div->receive_data(create_message<NumberData>(1, NumberData{2.0}), 1);
    div->receive_data(create_message<NumberData>(2, NumberData{4.0}), 1);

    WHEN("State is serialized and restored") {
      Bytes state = div->collect();
      auto restored = std::make_unique<Division>("div1");
      Bytes::const_iterator it = state.begin();
      restored->restore(it);

      THEN("Behavior matches original") {
        restored->execute();
        const auto& output = restored->get_output_queue(0);
        REQUIRE(output.size() == 2);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1->time == 1);
        REQUIRE(msg1->data.value == 5.0);

        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2->time == 2);
        REQUIRE(msg2->data.value == 5.0);
      }
    }
  }
}