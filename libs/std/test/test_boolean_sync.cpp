#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/std/BooleanSync.h"

using namespace rtbot;

SCENARIO("BooleanSync operators handle basic operations", "[boolean_sync_binary_op]") {
  GIVEN("Basic logical operators") {
    auto and_op = make_logical_and("and1");
    auto or_op = make_logical_or("or1");
    auto xor_op = make_logical_xor("xor1");
    auto nand_op = make_logical_nand("nand1");
    auto nor_op = make_logical_nor("nor1");
    auto xnor_op = make_logical_xnor("xnor1");
    auto impl_op = make_logical_implication("impl1");

    WHEN("Testing truth table combinations") {
      struct TestCase {
        bool a;
        bool b;
        bool and_result;
        bool or_result;
        bool xor_result;
        bool nand_result;
        bool nor_result;
        bool xnor_result;
        bool impl_result;
      };

      std::vector<TestCase> test_cases = {{false, false, false, false, false, true, true, true, true},
                                          {false, true, false, true, true, true, false, false, true},
                                          {true, false, false, true, true, true, false, false, false},
                                          {true, true, true, true, false, false, false, true, true}};

      timestamp_t time = 0;
      for (const auto& test : test_cases) {
        time += 1;
        // Send messages with same timestamp
        auto send_inputs = [time](auto& op, bool a, bool b) {
          op->receive_data(create_message<BooleanData>(time, BooleanData{a}), 0);
          op->receive_data(create_message<BooleanData>(time, BooleanData{b}), 1);
          op->execute();
        };

        send_inputs(and_op, test.a, test.b);
        send_inputs(or_op, test.a, test.b);
        send_inputs(xor_op, test.a, test.b);
        send_inputs(nand_op, test.a, test.b);
        send_inputs(nor_op, test.a, test.b);
        send_inputs(xnor_op, test.a, test.b);
        send_inputs(impl_op, test.a, test.b);

        THEN("Operations produce correct results for input combination") {
          INFO("Testing with a=" << test.a << ", b=" << test.b);

          auto check_output = [](const auto& op, bool expected) {
            const auto& output = op->get_output_queue(0);
            REQUIRE(output.size() == 1);
            const auto* msg = dynamic_cast<const Message<BooleanData>*>(output.front().get());
            REQUIRE(msg->time == 1);
            REQUIRE(msg->data.value == expected);
          };

          INFO("Testing AND");
          check_output(and_op, test.and_result);
          INFO("Testing OR");
          check_output(or_op, test.or_result);
          INFO("Testing XOR");
          check_output(xor_op, test.xor_result);
          INFO("Testing NAND");
          check_output(nand_op, test.nand_result);
          INFO("Testing NOR");
          check_output(nor_op, test.nor_result);
          INFO("Testing XNOR");
          check_output(xnor_op, test.xnor_result);
          INFO("Testing IMPL");
          check_output(impl_op, test.impl_result);
        }
      }
    }
  }
}

SCENARIO("BooleanSync operators handle unsynchronized messages", "[boolean_sync_binary_op]") {
  GIVEN("A logical AND operator") {
    auto and_op = make_logical_and("and1");

    WHEN("Receiving messages with different timestamps") {
      and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
      and_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);
      and_op->execute();

      THEN("No output is produced") { REQUIRE(and_op->get_output_queue(0).empty()); }
    }
  }
}

SCENARIO("BooleanSync operators handle state serialization", "[boolean_sync_binary_op]") {
  GIVEN("An operator with buffered messages") {
    auto and_op = make_logical_and("and1");

    // Add some messages to buffer
    and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    and_op->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
    and_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = and_op->collect();

      // Create new operator
      auto restored = make_logical_and("and1");

      // Restore state
      Bytes::const_iterator it = state.begin();
      restored->restore(it);

      // Execute both operators
      and_op->execute();
      restored->execute();

      THEN("Both operators produce identical results") {
        const auto& orig_output = and_op->get_output_queue(0);
        const auto& rest_output = restored->get_output_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());

        for (size_t i = 0; i < orig_output.size(); i++) {
          const auto* orig_msg = dynamic_cast<const Message<BooleanData>*>(orig_output[i].get());
          const auto* rest_msg = dynamic_cast<const Message<BooleanData>*>(rest_output[i].get());

          REQUIRE(orig_msg->time == rest_msg->time);
          REQUIRE(orig_msg->data.value == rest_msg->data.value);
        }
      }
    }
  }
}

SCENARIO("BooleanSync operators handle empty and single message cases", "[boolean_sync_binary_op]") {
  GIVEN("A logical AND operator") {
    auto and_op = make_logical_and("and1");

    WHEN("No messages are received") {
      and_op->execute();

      THEN("No output is produced") { REQUIRE(and_op->get_output_queue(0).empty()); }
    }

    WHEN("Only one input port receives a message") {
      and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
      and_op->execute();

      THEN("No output is produced") { REQUIRE(and_op->get_output_queue(0).empty()); }
    }
  }
}
