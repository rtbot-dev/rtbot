#include <catch2/catch.hpp>
#include <cmath>
#include <memory>
#include <vector>

#include "rtbot/std/Replace.h"

using namespace rtbot;

SCENARIO("Replace derived classes handle basic filtering", "[replace_op]") {
  SECTION("LessThanOrEqualToReplace operator") {
    auto ltR = make_less_than_or_equal_to_replace("ltR", 3.0, 1.0);

    REQUIRE(ltR->type_name() == "LessThanOrEqualToReplace");
    REQUIRE(dynamic_cast<LessThanOrEqualToReplace*>(ltR.get())->get_threshold() == 3.0);
    REQUIRE(dynamic_cast<LessThanOrEqualToReplace*>(ltR.get())->get_replace_by() == 1.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 1.0},  // Should be replaced
        {2, 4.0},  // maintain
        {4, 2.5},  // Should be replaced
        {5, 3.0}   // Should be replaced
    };

    std::vector<std::pair<timestamp_t, double>> expected = {{1, 1.0}, {2, 4.0}, {4, 1.0}, {5, 1.0}};

    for (const auto& input : inputs) {
      ltR->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    ltR->execute();

    auto& output = ltR->get_output_queue(0);
    REQUIRE(output.size() == expected.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }
}

SCENARIO("LessThanOrEqualToReplace handles edge cases", "[replace_op]") {
  SECTION("NaN values") {
    auto ltR = make_less_than_or_equal_to_replace("ltR", 1.0, 0.0);

    ltR->receive_data(create_message<NumberData>(1, NumberData{std::numeric_limits<double>::quiet_NaN()}), 0);
    ltR->execute();

    auto& output = ltR->get_output_queue(0);
    REQUIRE(output.empty());  // NaN comparisons should fail
  }

  SECTION("Infinity values") {
    auto ltR = make_less_than_or_equal_to_replace("ltR", std::numeric_limits<double>::infinity(), 0.0);

    ltR->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    ltR->execute();

    auto& output = ltR->get_output_queue(0);
    REQUIRE(!output.empty());  // Finite values should be less than infinity
  }
}

SCENARIO("LessThanOrEqualToReplace handles error cases", "[replace_op]") {
  SECTION("Invalid message type") {
    auto ltR = make_less_than_or_equal_to_replace("ltR", 3.0, 1.9);

    REQUIRE_THROWS_AS(ltR->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
  }

  SECTION("Invalid port index") {
    auto ltR = make_less_than_or_equal_to_replace("ltR", 3.0, 9.0);

    REQUIRE_THROWS_AS(ltR->receive_data(create_message<NumberData>(1, NumberData{1.0}), 1), std::runtime_error);
  }
}

SCENARIO("ReplaceOp handles serialization", "[replace_op]") {
  SECTION("LessThanOrEqualToReplace operator serialization") {
    auto ltR = make_less_than_or_equal_to_replace("ltR", 3.0, 2.0);

    // Fill with some data
    ltR->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    ltR->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
    ltR->execute();

    // Serialize state
    Bytes state = ltR->collect();

    // Create new operator and restore state
    auto restored = make_less_than_or_equal_to_replace("ltR", 3.0, 2.0);
    auto it = state.cbegin();
    restored->restore(it);

    // Verify restored state
    REQUIRE(restored->type_name() == ltR->type_name());
    REQUIRE(dynamic_cast<LessThanOrEqualToReplace*>(restored.get())->get_threshold() ==
            dynamic_cast<LessThanOrEqualToReplace*>(ltR.get())->get_threshold());
    REQUIRE(dynamic_cast<LessThanOrEqualToReplace*>(restored.get())->get_replace_by() ==
            dynamic_cast<LessThanOrEqualToReplace*>(ltR.get())->get_replace_by());

    // Process new data and verify behavior
    restored->clear_all_output_ports();
    restored->receive_data(create_message<NumberData>(3, NumberData{1.0}), 0);
    restored->execute();

    auto& output = restored->get_output_queue(0);
    REQUIRE(!output.empty());
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg->time == 3);
    REQUIRE(msg->data.value == 2.0);
  }
}