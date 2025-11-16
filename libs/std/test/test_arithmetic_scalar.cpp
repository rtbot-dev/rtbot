#include <catch2/catch.hpp>
#include <cmath>
#include <memory>
#include <vector>

#include "rtbot/std/ArithmeticScalar.h"

using namespace rtbot;

SCENARIO("ArithmeticScalar derived classes handle basic operations", "[math_scalar_op]") {
  SECTION("Add operator") {
    auto add = make_add("add1", 2.0);

    REQUIRE(add->type_name() == "Add");
    REQUIRE(dynamic_cast<Add*>(add.get())->get_value() == 2.0);

    // Test single value
    add->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    add->execute();

    auto& output = add->get_output_queue(0);
    REQUIRE(output.size() == 1);
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg->time == 1);
    REQUIRE(msg->data.value == 3.0);

    // Test sequence of values
    std::vector<std::pair<timestamp_t, double>> inputs = {{2, -1.0}, {4, 0.0}, {5, 10.0}};

    std::vector<std::pair<timestamp_t, double>> expected = {{2, 1.0}, {4, 2.0}, {5, 12.0}};

    add->clear_all_output_ports();

    for (const auto& input : inputs) {
      add->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
      add->execute();
    }

    auto& output_queue = add->get_output_queue(0);
    REQUIRE(output_queue.size() == inputs.size());

    for (size_t i = 0; i < output_queue.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output_queue[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = add->collect();

      REQUIRE(add->get_output_queue(0).size() == 3);
      
      auto restored = make_add("add1", 2.0);

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The operators match") {
        REQUIRE(*restored == *add);
      }
    }
  }

  SECTION("Scale operator") {
    auto scale = make_scale("scale1", 2.0);

    REQUIRE(scale->type_name() == "Scale");
    REQUIRE(dynamic_cast<Scale*>(scale.get())->get_value() == 2.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 1.0}, {2, -1.0}, {4, 0.5}};

    std::vector<std::pair<timestamp_t, double>> expected = {{1, 2.0}, {2, -2.0}, {4, 1.0}};

    for (const auto& input : inputs) {
      scale->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    scale->execute();

    auto& output = scale->get_output_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = scale->collect();

      REQUIRE(scale->get_output_queue(0).size() == 3);
      
      auto restored = make_scale("scale1", 2.0);

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The operators match") {
        REQUIRE(*restored == *scale);
      }
    }
  }

  SECTION("Power operator") {
    auto power = make_power("pow1", 2.0);

    REQUIRE(power->type_name() == "Power");
    REQUIRE(dynamic_cast<Power*>(power.get())->get_value() == 2.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 2.0}, {2, -2.0}, {4, 0.5}};

    std::vector<std::pair<timestamp_t, double>> expected = {{1, 4.0}, {2, 4.0}, {4, 0.25}};

    for (const auto& input : inputs) {
      power->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    power->execute();

    auto& output = power->get_output_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == Approx(expected[i].second));
    }

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = power->collect();

      REQUIRE(power->get_output_queue(0).size() == 3);
      
      auto restored = make_power("pow1", 2.0);

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The operators match") {
        REQUIRE(*restored == *power);
      }
    }
  }
}

SCENARIO("ArithmeticScalar handles trigonometric functions", "[math_scalar_op]") {
  SECTION("Sin operator") {
    auto sin = make_sin("sin1");

    REQUIRE(sin->type_name() == "Sin");

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 0.0}, {2, M_PI / 2}, {4, M_PI}};

    std::vector<std::pair<timestamp_t, double>> expected = {{1, 0.0}, {2, 1.0}, {4, 0.0}};

    for (const auto& input : inputs) {
      sin->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    sin->execute();

    auto& output = sin->get_output_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
    }

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = sin->collect();

      REQUIRE(sin->get_output_queue(0).size() == 3);
      
      auto restored = make_sin("sin1");

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The operators match") {
        REQUIRE(*restored == *sin);
      }
    }
  }
}

SCENARIO("ArithmeticScalar handles exponential and logarithmic functions", "[math_scalar_op]") {
  SECTION("Exp operator") {
    auto exp = make_exp("exp1");

    REQUIRE(exp->type_name() == "Exp");

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 0.0}, {2, 1.0}, {4, -1.0}};

    std::vector<std::pair<timestamp_t, double>> expected = {{1, 1.0}, {2, M_E}, {4, 1.0 / M_E}};

    for (const auto& input : inputs) {
      exp->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    exp->execute();

    auto& output = exp->get_output_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == Approx(expected[i].second));
    }

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = exp->collect();

      REQUIRE(exp->get_output_queue(0).size() == 3);
      
      auto restored = make_exp("exp1");

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The operators match") {
        REQUIRE(*restored == *exp);
      }
    }
  }

  // Similar tests for Log and Log10...
}

SCENARIO("ArithmeticScalar handles rounding functions", "[math_scalar_op]") {
  SECTION("Round operator") {
    auto round = make_round("round1");

    REQUIRE(round->type_name() == "Round");

    std::vector<std::pair<timestamp_t, double>> inputs = {{1, 1.4}, {2, 1.6}, {4, -1.5}, {5, -1.6}};

    std::vector<std::pair<timestamp_t, double>> expected = {{1, 1.0}, {2, 2.0}, {4, -2.0}, {5, -2.0}};

    for (const auto& input : inputs) {
      round->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }

    round->execute();
    auto& output = round->get_output_queue(0);
    REQUIRE(output.size() == inputs.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = round->collect();

      REQUIRE(round->get_output_queue(0).size() == 4);
      
      auto restored = make_round("round1");

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("The operators match") {
        REQUIRE(*restored == *round);
      }
    }

  }

  // Similar tests for Floor and Ceil...
}

SCENARIO("ArithmeticScalar handles serialization", "[math_scalar_op][State]") {
  SECTION("Add operator serialization") {
    auto add = make_add("add1", 2.0);

    // Fill with some data
    add->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    add->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    add->execute();

    // Serialize state
    Bytes state = add->collect();

    // Create new operator
    auto restored = make_add("add1", 2.0);

    // Restore state
    auto it = state.cbegin();
    restored->restore(it);

    // Verify restored state
    REQUIRE(restored->type_name() == add->type_name());
    REQUIRE(dynamic_cast<Add*>(restored.get())->get_value() == dynamic_cast<Add*>(add.get())->get_value());
    REQUIRE(*restored == *add);

    // Process new data and verify behavior
    restored->clear_all_output_ports();
    restored->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
    restored->execute();

    auto& output = restored->get_output_queue(0);
    REQUIRE(!output.empty());
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg->time == 3);
    REQUIRE(msg->data.value == 5.0);
  }

  // Similar tests for other operators...
}

SCENARIO("ArithmeticScalar handles error cases", "[math_scalar_op]") {
  SECTION("Invalid message type") {
    auto add = make_add("add1", 2.0);

    REQUIRE_THROWS_AS(add->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
  }

  SECTION("Invalid port index") {
    auto add = make_add("add1", 2.0);

    REQUIRE_THROWS_AS(add->receive_data(create_message<NumberData>(1, NumberData{1.0}), 1), std::runtime_error);
  }

  SECTION("Log of negative number") {
    auto log = make_log("log1");

    log->receive_data(create_message<NumberData>(1, NumberData{-1.0}), 0);
    log->execute();

    auto& output = log->get_output_queue(0);
    REQUIRE(!output.empty());
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(std::isnan(msg->data.value));
  }
}