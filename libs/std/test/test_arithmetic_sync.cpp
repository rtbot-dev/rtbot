#include <catch2/catch.hpp>

#include "rtbot/std/ArithmeticSync.h"

using namespace rtbot;

SCENARIO("ArithmeticSync operators handle basic synchronization", "[math_sync_binary_op]") {
  GIVEN("Basic mathematical operators") {
    auto add = make_addition("add1");
    auto sub = make_subtraction("sub1");
    auto mul = make_multiplication("mul1");
    auto div = make_division("div1");

    WHEN("Receiving synchronized messages") {
      // Send messages with same timestamp
      add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      add->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);

      sub->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      sub->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);

      mul->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      mul->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);

      div->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      div->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);

      add->execute();
      sub->execute();
      mul->execute();
      div->execute();

      THEN("Operations produce correct results") {
        auto check_output = [](const auto& op, double expected) {
          const auto& output = op->get_output_queue(0);
          REQUIRE(output.size() == 1);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg->time == 1);
          REQUIRE(msg->data.value == Approx(expected));
        };

        check_output(add, 15.0);  // 10 + 5
        check_output(sub, 5.0);   // 10 - 5
        check_output(mul, 50.0);  // 10 * 5
        check_output(div, 2.0);   // 10 / 5
      }
    }
  }
}

SCENARIO("ArithmeticSync operators handle unsynchronized messages", "[math_sync_binary_op]") {
  GIVEN("An addition operator") {
    auto add = make_addition("add1");

    WHEN("Receiving messages with different timestamps") {
      add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      add->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
      add->execute();

      THEN("No output is produced") { REQUIRE(add->get_output_queue(0).empty()); }
    }
  }
}
// TODO: check division by zero test
/*
SCENARIO("Division operator handles division by zero", "[math_sync_binary_op]") {
  GIVEN("A division operator") {
    auto div = make_division("div1");

    WHEN("Dividing by zero") {
      div->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      div->receive_data(create_message<NumberData>(1, NumberData{0.0}), 1);
      div->execute();

      THEN("No output is produced") { REQUIRE(div->get_output_queue(0).empty()); }
    }
  }
}*/

SCENARIO("ArithmeticSync operators handle state serialization", "[math_sync_binary_op]") {
  GIVEN("An operator with buffered messages") {
    auto add = make_addition("add1");

    // Add some messages to buffer
    add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    add->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
    add->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    add->receive_data(create_message<NumberData>(2, NumberData{10.0}), 1);

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = add->collect();

      // Create new operator
      auto restored = make_addition("add1");

      // Restore state
      Bytes::const_iterator it = state.begin();
      restored->restore(it);

      // Execute both operators
      add->execute();
      restored->execute();

      THEN("Both operators produce identical results") {
        const auto& orig_output = add->get_output_queue(0);
        const auto& rest_output = restored->get_output_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());

        for (size_t i = 0; i < orig_output.size(); i++) {
          const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[i].get());
          const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[i].get());

          REQUIRE(orig_msg->time == rest_msg->time);
          REQUIRE(orig_msg->data.value == rest_msg->data.value);
        }
      }
    }
  }
}

SCENARIO("ArithmeticSync operators handle multiple inputs", "[math_sync_binary_op]") {
  GIVEN("Arithmetic operators with 3 inputs") {
    auto add = make_addition("add1", 3);
    auto mul = make_multiplication("mul1", 3);
    auto div = make_division("div1", 3);

    WHEN("Receiving synchronized messages") {
      // Send messages with same timestamp
      add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      add->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      add->receive_data(create_message<NumberData>(1, NumberData{2.0}), 2);

      mul->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      mul->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      mul->receive_data(create_message<NumberData>(1, NumberData{2.0}), 2);

      div->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      div->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      div->receive_data(create_message<NumberData>(1, NumberData{2.0}), 2);

      add->execute();
      mul->execute();
      div->execute();

      THEN("Operations produce correct results") {
        auto check_output = [](const auto& op, double expected) {
          const auto& output = op->get_output_queue(0);
          REQUIRE(output.size() == 1);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg->time == 1);
          REQUIRE(msg->data.value == Approx(expected));
        };

        check_output(add, 17.0);   // 10 + 5 + 2
        check_output(mul, 100.0);  // 10 * 5 * 2
        check_output(div, 1.0);    // 10 / (5 * 2)
      }
    }

    WHEN("One input is missing") {
      add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      add->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      add->execute();

      THEN("No output is produced") { REQUIRE(add->get_output_queue(0).empty()); }
    }
  }
}