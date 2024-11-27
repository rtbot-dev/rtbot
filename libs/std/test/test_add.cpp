#include <catch2/catch.hpp>

#include "rtbot/std/Add.h"

using namespace rtbot;

SCENARIO("Add operator handles basic addition", "[add]") {
  GIVEN("An Add operator that adds 5.0") {
    auto add = make_add("add1", 5.0);

    WHEN("Receiving a number message") {
      add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      add->execute();

      THEN("Value is correctly added") {
        const auto& output = add->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 15.0);
      }
    }

    WHEN("Receiving multiple messages") {
      add->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      add->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      add->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      add->execute();

      THEN("All messages are processed correctly") {
        const auto& output = add->get_output_queue(0);
        REQUIRE(output.size() == 3);

        auto it = output.begin();
        for (int i = 0; i < 3; ++i) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(it->get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == i + 1);
          REQUIRE(msg->data.value == (i + 1) + 5.0);
          ++it;
        }
      }
    }
  }
}

SCENARIO("Add operator handles edge cases", "[add]") {
  GIVEN("An Add operator that adds 0.0") {
    auto add = make_add("add2", 0.0);

    WHEN("Adding zero") {
      add->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      add->execute();

      THEN("Original value is preserved") {
        const auto& output = add->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg->data.value == 42.0);
      }
    }
  }

  GIVEN("An Add operator that adds a negative number") {
    auto add = make_add("add3", -5.0);

    WHEN("Adding a negative number") {
      add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      add->execute();

      THEN("Subtraction is performed correctly") {
        const auto& output = add->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg->data.value == 5.0);
      }
    }
  }
}

SCENARIO("Add operator validates message types", "[add]") {
  GIVEN("An Add operator") {
    auto add = make_add("add4", 1.0);

    WHEN("Receiving wrong message type") {
      THEN("Type mismatch is detected") {
        REQUIRE_THROWS_AS(add->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
      }
    }
  }
}

SCENARIO("Add operator handles state serialization", "[add]") {
  GIVEN("An Add operator with some processed messages") {
    auto add = make_add("add5", 5.0);
    add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    add->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = add->collect();

      // Create new operator
      auto restored = make_add("add5", 0.0);  // Different value to ensure it gets restored

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Value is correctly restored") { REQUIRE(restored->get_value() == 5.0); }

      AND_WHEN("Processing new messages") {
        restored->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
        restored->execute();

        THEN("Operator works with restored state") {
          const auto& output = restored->get_output_queue(0);
          REQUIRE(output.size() == 1);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg->data.value == 25.0);
        }
      }
    }
  }
}