#include <catch2/catch.hpp>
#include <cmath>
#include <memory>
#include <vector>

#include "rtbot/std/FilterScalarOp.h"

using namespace rtbot;

SCENARIO("FilterScalarOp derived classes handle basic filtering", "[filter_scalar_op]") {
  SECTION("LessThan operator") {
    auto lt = make_less_than("lt1", 3.0);

    REQUIRE(lt->type_name() == "LessThan");
    REQUIRE(dynamic_cast<LessThan*>(lt.get())->get_threshold() == 3.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 1.0},  // Should pass
        {2, 4.0},  // Should be filtered
        {4, 2.5},  // Should pass
        {5, 3.0}   // Should be filtered (not strictly less than)
    };

    std::vector<std::pair<timestamp_t, double>> expected = {{1, 1.0}, {4, 2.5}};

    for (const auto& input : inputs) {
      lt->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    lt->execute();

    auto& output = lt->get_output_queue(0);
    REQUIRE(output.size() == expected.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }

  SECTION("GreaterThan operator") {
    auto gt = make_greater_than("gt1", 3.0);

    REQUIRE(gt->type_name() == "GreaterThan");
    REQUIRE(dynamic_cast<GreaterThan*>(gt.get())->get_threshold() == 3.0);

    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 1.0},  // Should be filtered
        {2, 4.0},  // Should pass
        {4, 2.5},  // Should be filtered
        {5, 3.0}   // Should be filtered (not strictly greater than)
    };

    std::vector<std::pair<timestamp_t, double>> expected = {{2, 4.0}};

    for (const auto& input : inputs) {
      gt->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    gt->execute();

    auto& output = gt->get_output_queue(0);
    REQUIRE(output.size() == expected.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }

  SECTION("EqualTo operator") {
    auto eq = make_equal_to("eq1", 3.0, 0.1);

    REQUIRE(eq->type_name() == "EqualTo");
    REQUIRE(dynamic_cast<EqualTo*>(eq.get())->get_value() == 3.0);
    REQUIRE(dynamic_cast<EqualTo*>(eq.get())->get_epsilon() == 0.1);

    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 1.0},   // Should be filtered
        {2, 2.95},  // Should pass (within epsilon)
        {4, 3.05},  // Should pass (within epsilon)
        {5, 3.2}    // Should be filtered
    };

    std::vector<std::pair<timestamp_t, double>> expected = {{2, 2.95}, {4, 3.05}};

    for (const auto& input : inputs) {
      eq->receive_data(create_message<NumberData>(input.first, NumberData{input.second}), 0);
    }
    eq->execute();

    auto& output = eq->get_output_queue(0);
    REQUIRE(output.size() == expected.size());

    for (size_t i = 0; i < output.size(); ++i) {
      auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }
}

SCENARIO("FilterScalarOp handles edge cases", "[filter_scalar_op]") {
  SECTION("NaN values") {
    auto gt = make_greater_than("gt1", 0.0);

    gt->receive_data(create_message<NumberData>(1, NumberData{std::numeric_limits<double>::quiet_NaN()}), 0);
    gt->execute();

    auto& output = gt->get_output_queue(0);
    REQUIRE(output.empty());  // NaN comparisons should fail
  }

  SECTION("Infinity values") {
    auto lt = make_less_than("lt1", std::numeric_limits<double>::infinity());

    lt->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    lt->execute();

    auto& output = lt->get_output_queue(0);
    REQUIRE(!output.empty());  // Finite values should be less than infinity
  }

  SECTION("Exact equality with zero epsilon") {
    auto eq = make_equal_to("eq1", 3.0, 0.0);

    eq->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
    eq->receive_data(create_message<NumberData>(2, NumberData{3.0 + 1e-15}), 0);
    eq->execute();

    auto& output = eq->get_output_queue(0);
    REQUIRE(output.size() == 1);  // Only exact match should pass
  }
}

SCENARIO("FilterScalarOp handles error cases", "[filter_scalar_op]") {
  SECTION("Invalid message type") {
    auto lt = make_less_than("lt1", 3.0);

    REQUIRE_THROWS_AS(lt->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
  }

  SECTION("Invalid port index") {
    auto lt = make_less_than("lt1", 3.0);

    REQUIRE_THROWS_AS(lt->receive_data(create_message<NumberData>(1, NumberData{1.0}), 1), std::runtime_error);
  }
}

SCENARIO("FilterScalarOp handles serialization", "[filter_scalar_op]") {
  SECTION("LessThan operator serialization") {
    auto lt = make_less_than("lt1", 3.0);

    // Fill with some data
    lt->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    lt->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
    lt->execute();

    // Serialize state
    Bytes state = lt->collect();

    // Create new operator and restore state
    auto restored = make_less_than("lt1", 3.0);
    auto it = state.cbegin();
    restored->restore(it);

    // Verify restored state
    REQUIRE(restored->type_name() == lt->type_name());
    REQUIRE(dynamic_cast<LessThan*>(restored.get())->get_threshold() ==
            dynamic_cast<LessThan*>(lt.get())->get_threshold());

    // Process new data and verify behavior
    restored->clear_all_output_ports();
    restored->receive_data(create_message<NumberData>(3, NumberData{2.0}), 0);
    restored->execute();

    auto& output = restored->get_output_queue(0);
    REQUIRE(!output.empty());
    auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg->time == 3);
    REQUIRE(msg->data.value == 2.0);
  }
}