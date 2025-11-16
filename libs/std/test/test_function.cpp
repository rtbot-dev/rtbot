#include <catch2/catch.hpp>
#include <cmath>

#include "rtbot/std/Function.h"

using namespace rtbot;

SCENARIO("Function operator handles basic interpolation", "[function]") {
  GIVEN("A linear function with two points") {
    std::vector<std::pair<double, double>> points = {{0.0, 0.0}, {1.0, 1.0}};
    auto func = std::make_unique<Function>("func", points);

    WHEN("Processing points within range") {
      func->receive_data(create_message<NumberData>(1, NumberData{0.5}), 0);
      func->execute();

      THEN("Linear interpolation is correct") {
        const auto& output = func->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == Approx(0.5));
      }
    }

    WHEN("Processing points outside range") {
      func->receive_data(create_message<NumberData>(1, NumberData{-1.0}), 0);
      func->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      func->execute();

      THEN("Extrapolation is performed correctly") {
        const auto& output = func->get_output_queue(0);
        REQUIRE(output.size() == 2);
        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg1->data.value == Approx(-1.0));
        REQUIRE(msg2->data.value == Approx(2.0));
      }
    }
  }

  GIVEN("A function with Hermite interpolation") {
    std::vector<std::pair<double, double>> points = {{0.0, 0.0}, {1.0, 1.0}, {2.0, 0.0}};
    auto func = std::make_unique<Function>("func", points, InterpolationType::HERMITE);

    WHEN("Processing points") {
      func->receive_data(create_message<NumberData>(1, NumberData{0.5}), 0);
      func->execute();

      THEN("Hermite interpolation is performed") {
        const auto& output = func->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value > 0.0);  // Should overshoot linear interpolation
      }
    }
  }
}

SCENARIO("Function operator handles edge cases", "[function]") {
  SECTION("Invalid point count") {
    std::vector<std::pair<double, double>> single_point = {{0.0, 0.0}};
    REQUIRE_THROWS_AS(Function("func", single_point), std::runtime_error);
  }

  SECTION("Unsorted points are automatically sorted") {
    std::vector<std::pair<double, double>> unsorted_points = {{1.0, 1.0}, {0.0, 0.0}};
    auto func = std::make_unique<Function>("func", unsorted_points);
    const auto& points = func->get_points();
    REQUIRE(points[0].first == 0.0);
    REQUIRE(points[1].first == 1.0);
  }
}

SCENARIO("Function operator handles state serialization", "[function][State]") {
  GIVEN("a valid function") {
    std::vector<std::pair<double, double>> points = {{0.0, 0.0},{1.0, 1.0},{2.0, 2.0}};
    auto function = std::make_shared<Function>("func", points);
    function->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    function->execute();
    function->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);

    Bytes state = function->collect();
    auto restored = std::make_shared<Function>("func", points);
    auto it = state.cbegin();
    restored->restore(it);

    SECTION("verifying deserialization") {
      REQUIRE(*function == *restored);
    }
  }
}

SCENARIO("Function operator processes messages in sequence", "[function]") {
  GIVEN("A linear function with multiple points") {
    std::vector<std::pair<double, double>> points = {{0.0, 0.0}, {1.0, 2.0}, {2.0, 4.0}};
    auto func = std::make_unique<Function>("func", points);

    WHEN("Processing messages with different timestamps") {
      func->receive_data(create_message<NumberData>(1, NumberData{0.5}), 0);
      func->receive_data(create_message<NumberData>(2, NumberData{1.5}), 0);
      func->receive_data(create_message<NumberData>(3, NumberData{1.0}), 0);
      func->execute();

      THEN("Messages are processed in order with correct timestamps") {
        const auto& output = func->get_output_queue(0);
        REQUIRE(output.size() == 3);

        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        const auto* msg3 = dynamic_cast<const Message<NumberData>*>(output[2].get());

        REQUIRE(msg1->time == 1);
        REQUIRE(msg2->time == 2);
        REQUIRE(msg3->time == 3);

        REQUIRE(msg1->data.value == Approx(1.0));  // 0.5 -> 1.0
        REQUIRE(msg2->data.value == Approx(3.0));  // 1.5 -> 3.0
        REQUIRE(msg3->data.value == Approx(2.0));  // 1.0 -> 2.0
      }
    }
  }
}