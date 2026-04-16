#include <catch2/catch.hpp>

#include "rtbot/Collector.h"
#include "rtbot/std/ArithmeticSync.h"

using namespace rtbot;

SCENARIO("ArithmeticSync operators handle basic synchronization", "[math_sync_binary_op]") {
  GIVEN("Basic mathematical operators") {
    auto add = make_addition("add1");
    auto sub = make_subtraction("sub1");
    auto mul = make_multiplication("mul1");
    auto div = make_division("div1");

    auto add_col = std::make_shared<Collector>("add_c", std::vector<std::string>{"number"});
    auto sub_col = std::make_shared<Collector>("sub_c", std::vector<std::string>{"number"});
    auto mul_col = std::make_shared<Collector>("mul_c", std::vector<std::string>{"number"});
    auto div_col = std::make_shared<Collector>("div_c", std::vector<std::string>{"number"});
    add->connect(add_col, 0, 0);
    sub->connect(sub_col, 0, 0);
    mul->connect(mul_col, 0, 0);
    div->connect(div_col, 0, 0);

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
        auto check_output = [](const auto& col, double expected) {
          const auto& output = col->get_data_queue(0);
          REQUIRE(output.size() == 1);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg->time == 1);
          REQUIRE(msg->data.value == Approx(expected));
        };

        check_output(add_col, 15.0);  // 10 + 5
        check_output(sub_col, 5.0);   // 10 - 5
        check_output(mul_col, 50.0);  // 10 * 5
        check_output(div_col, 2.0);   // 10 / 5
      }
    }

    WHEN("Additon State is serialized and restored") {
      // Serialize state
      add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      add->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      add->execute();
      auto state = add->collect();

      REQUIRE(add_col->get_data_queue(0).size() == 1);

      auto restored = make_addition("add1");
      auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);

      // Restore state
      restored->restore_data_from_json(state);

      THEN("The operators match") { REQUIRE(*restored == *add); }
    }

    WHEN("Subtraction State is serialized and restored") {
      // Serialize state
      sub->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      sub->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      sub->execute();
      auto state = sub->collect();

      REQUIRE(sub_col->get_data_queue(0).size() == 1);

      auto restored = make_subtraction("sub1");
      auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);

      // Restore state
      restored->restore_data_from_json(state);

      THEN("The operators match") { REQUIRE(*restored == *sub); }
    }

    WHEN("Multiplication State is serialized and restored") {
      // Serialize state
      mul->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      mul->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      mul->execute();
      auto state = mul->collect();

      REQUIRE(mul_col->get_data_queue(0).size() == 1);

      auto restored = make_multiplication("mul1");
      auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);

      // Restore state
      restored->restore_data_from_json(state);

      THEN("The operators match") { REQUIRE(*restored == *mul); }
    }

    WHEN("Division State is serialized and restored") {
      // Serialize state
      div->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      div->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      div->execute();
      auto state = div->collect();

      REQUIRE(div_col->get_data_queue(0).size() == 1);

      auto restored = make_division("div1");
      auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);

      // Restore state
      restored->restore_data_from_json(state);

      THEN("The operators match") { REQUIRE(*restored == *div); }
    }

  }
}

SCENARIO("ArithmeticSync operators handle unsynchronized messages", "[math_sync_binary_op]") {
  GIVEN("An addition operator") {
    auto add = make_addition("add1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    add->connect(col, 0, 0);

    WHEN("Receiving messages with different timestamps") {
      add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      add->receive_data(create_message<NumberData>(2, NumberData{5.0}), 1);
      add->execute();

      THEN("No output is produced") { REQUIRE(col->get_data_queue(0).empty()); }
    }
  }
}
SCENARIO("ArithmeticSync operators handle state serialization", "[math_sync_binary_op][State]") {
  GIVEN("An operator with buffered messages") {
    auto add = make_addition("add1");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    add->connect(col, 0, 0);

    // Add some messages to buffer
    add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    add->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
    add->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
    add->receive_data(create_message<NumberData>(2, NumberData{10.0}), 1);

    WHEN("State is serialized and restored") {
      // Serialize state
      auto state = add->collect();

      // Create new operator
      auto restored = make_addition("add1");
      auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);

      // Restore state
      restored->restore_data_from_json(state);

      // Execute both operators
      add->execute();
      restored->execute();

      THEN("Both operators produce identical results") {
        REQUIRE(*add == *restored);
        const auto& orig_output = col->get_data_queue(0);
        const auto& rest_output = rcol->get_data_queue(0);

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

    auto add_col = std::make_shared<Collector>("add_c", std::vector<std::string>{"number"});
    auto mul_col = std::make_shared<Collector>("mul_c", std::vector<std::string>{"number"});
    auto div_col = std::make_shared<Collector>("div_c", std::vector<std::string>{"number"});
    add->connect(add_col, 0, 0);
    mul->connect(mul_col, 0, 0);
    div->connect(div_col, 0, 0);

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
        auto check_output = [](const auto& col, double expected) {
          const auto& output = col->get_data_queue(0);
          REQUIRE(output.size() == 1);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg->time == 1);
          REQUIRE(msg->data.value == Approx(expected));
        };

        check_output(add_col, 17.0);   // 10 + 5 + 2
        check_output(mul_col, 100.0);  // 10 * 5 * 2
        check_output(div_col, 1.0);    // 10 / (5 * 2)
      }
    }

    WHEN("One input is missing") {
      add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      add->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
      add->execute();

      THEN("No output is produced") { REQUIRE(add_col->get_data_queue(0).empty()); }
    }
  }
}
