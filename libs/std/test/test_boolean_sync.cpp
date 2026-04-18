#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/Collector.h"
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

    auto and_col = std::make_shared<Collector>("and_c", std::vector<std::string>{"boolean"});
    auto or_col = std::make_shared<Collector>("or_c", std::vector<std::string>{"boolean"});
    auto xor_col = std::make_shared<Collector>("xor_c", std::vector<std::string>{"boolean"});
    auto nand_col = std::make_shared<Collector>("nand_c", std::vector<std::string>{"boolean"});
    auto nor_col = std::make_shared<Collector>("nor_c", std::vector<std::string>{"boolean"});
    auto xnor_col = std::make_shared<Collector>("xnor_c", std::vector<std::string>{"boolean"});
    auto impl_col = std::make_shared<Collector>("impl_c", std::vector<std::string>{"boolean"});
    and_op->connect(and_col, 0, 0);
    or_op->connect(or_col, 0, 0);
    xor_op->connect(xor_col, 0, 0);
    nand_op->connect(nand_col, 0, 0);
    nor_op->connect(nor_col, 0, 0);
    xnor_op->connect(xnor_col, 0, 0);
    impl_op->connect(impl_col, 0, 0);

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

        // reset collectors from any prior iteration
        and_col->reset();
        or_col->reset();
        xor_col->reset();
        nand_col->reset();
        nor_col->reset();
        xnor_col->reset();
        impl_col->reset();

        send_inputs(and_op, test.a, test.b);
        send_inputs(or_op, test.a, test.b);
        send_inputs(xor_op, test.a, test.b);
        send_inputs(nand_op, test.a, test.b);
        send_inputs(nor_op, test.a, test.b);
        send_inputs(xnor_op, test.a, test.b);
        send_inputs(impl_op, test.a, test.b);

        THEN("Operations produce correct results for input combination") {
          INFO("Testing with a=" << test.a << ", b=" << test.b);

          auto check_output = [time](const auto& col, bool expected) {
            const auto& output = col->get_data_queue(0);
            REQUIRE(output.size() == 1);
            const auto* msg = dynamic_cast<const Message<BooleanData>*>(output.front().get());
            REQUIRE(msg->time == time);
            REQUIRE(msg->data.value == expected);
          };

          INFO("Testing AND");
          check_output(and_col, test.and_result);
          INFO("Testing OR");
          check_output(or_col, test.or_result);
          INFO("Testing XOR");
          check_output(xor_col, test.xor_result);
          INFO("Testing NAND");
          check_output(nand_col, test.nand_result);
          INFO("Testing NOR");
          check_output(nor_col, test.nor_result);
          INFO("Testing XNOR");
          check_output(xnor_col, test.xnor_result);
          INFO("Testing IMPL");
          check_output(impl_col, test.impl_result);
        }
      }
    }
  }
}

SCENARIO("BooleanSync operators handle unsynchronized messages", "[boolean_sync_binary_op]") {
  GIVEN("A logical AND operator") {
    auto and_op = make_logical_and("and1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    and_op->connect(col, 0, 0);

    WHEN("Receiving messages with different timestamps") {
      and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
      and_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);
      and_op->execute();

      THEN("No output is produced") { REQUIRE(col->get_data_queue(0).empty()); }
    }
  }
}

SCENARIO("BooleanSync operators handle state serialization", "[boolean_sync_binary_op][State]") {
  GIVEN("An operator with buffered messages") {
    auto and_op = make_logical_and("and1");
    auto or_op = make_logical_or("or1");
    auto xor_op = make_logical_xor("xor1");
    auto nand_op = make_logical_nand("nand1");
    auto nor_op = make_logical_nor("nor1");
    auto xnor_op = make_logical_xnor("xnor1");
    auto impl_op = make_logical_implication("impl1");

    auto and_col = std::make_shared<Collector>("and_c", std::vector<std::string>{"boolean"});
    auto or_col = std::make_shared<Collector>("or_c", std::vector<std::string>{"boolean"});
    auto xor_col = std::make_shared<Collector>("xor_c", std::vector<std::string>{"boolean"});
    auto nand_col = std::make_shared<Collector>("nand_c", std::vector<std::string>{"boolean"});
    auto nor_col = std::make_shared<Collector>("nor_c", std::vector<std::string>{"boolean"});
    auto xnor_col = std::make_shared<Collector>("xnor_c", std::vector<std::string>{"boolean"});
    auto impl_col = std::make_shared<Collector>("impl_c", std::vector<std::string>{"boolean"});
    and_op->connect(and_col, 0, 0);
    or_op->connect(or_col, 0, 0);
    xor_op->connect(xor_col, 0, 0);
    nand_op->connect(nand_col, 0, 0);
    nor_op->connect(nor_col, 0, 0);
    xnor_op->connect(xnor_col, 0, 0);
    impl_op->connect(impl_col, 0, 0);

    // Add some messages to buffer
    and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    and_op->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
    and_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);

    or_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    or_op->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    or_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
    or_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);

    xor_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    xor_op->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    xor_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
    xor_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);

    nand_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    nand_op->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    nand_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
    nand_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);

    nor_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    nor_op->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    nor_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
    nor_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);

    xnor_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    xnor_op->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    xnor_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
    xnor_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);

    impl_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
    impl_op->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
    impl_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
    impl_op->receive_data(create_message<BooleanData>(2, BooleanData{true}), 1);

    auto roundtrip_check = [](auto& orig, auto& orig_col, auto make_fn) {
      auto state = orig->collect();
      auto restored = make_fn();
      auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"boolean"});
      restored->connect(rcol, 0, 0);
      restored->restore_data_from_json(state);

      orig->execute();
      restored->execute();

      REQUIRE(*restored == *orig);
      const auto& orig_output = orig_col->get_data_queue(0);
      const auto& rest_output = rcol->get_data_queue(0);

      REQUIRE(orig_output.size() == rest_output.size());

      for (size_t i = 0; i < orig_output.size(); i++) {
        const auto* orig_msg = dynamic_cast<const Message<BooleanData>*>(orig_output[i].get());
        const auto* rest_msg = dynamic_cast<const Message<BooleanData>*>(rest_output[i].get());

        REQUIRE(orig_msg->time == rest_msg->time);
        REQUIRE(orig_msg->data.value == rest_msg->data.value);
      }
    };

    WHEN("and is serialized and restored") {
      THEN("Both operators produce identical results") {
        roundtrip_check(and_op, and_col, [] { return make_logical_and("and1"); });
      }
    }

    WHEN("or is serialized and restored") {
      THEN("Both operators produce identical results") {
        roundtrip_check(or_op, or_col, [] { return make_logical_or("or1"); });
      }
    }

    WHEN("xor is serialized and restored") {
      THEN("Both operators produce identical results") {
        roundtrip_check(xor_op, xor_col, [] { return make_logical_xor("xor1"); });
      }
    }

    WHEN("nand is serialized and restored") {
      THEN("Both operators produce identical results") {
        roundtrip_check(nand_op, nand_col, [] { return make_logical_nand("nand1"); });
      }
    }

    WHEN("nor is serialized and restored") {
      THEN("Both operators produce identical results") {
        roundtrip_check(nor_op, nor_col, [] { return make_logical_nor("nor1"); });
      }
    }

    WHEN("xnor is serialized and restored") {
      THEN("Both operators produce identical results") {
        roundtrip_check(xnor_op, xnor_col, [] { return make_logical_xnor("xnor1"); });
      }
    }

    WHEN("impl is serialized and restored") {
      THEN("Both operators produce identical results") {
        roundtrip_check(impl_op, impl_col, [] { return make_logical_implication("impl1"); });
      }
    }
  }
}

SCENARIO("BooleanSync operators handle empty and single message cases", "[boolean_sync_binary_op]") {
  GIVEN("A logical AND operator") {
    auto and_op = make_logical_and("and1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"boolean"});
    and_op->connect(col, 0, 0);

    WHEN("No messages are received") {
      and_op->execute();

      THEN("No output is produced") { REQUIRE(col->get_data_queue(0).empty()); }
    }

    WHEN("Only one input port receives a message") {
      and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
      and_op->execute();

      THEN("No output is produced") { REQUIRE(col->get_data_queue(0).empty()); }
    }
  }
}
