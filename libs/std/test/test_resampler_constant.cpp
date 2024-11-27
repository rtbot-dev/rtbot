#include <catch2/catch.hpp>

#include "rtbot/std/ResamplerConstant.h"

using namespace rtbot;

SCENARIO("ResamplerConstant initialization", "[ResamplerConstant]") {
  SECTION("Rejects invalid intervals") {
    REQUIRE_THROWS_AS(ResamplerConstant<NumberData>("test", 0), std::runtime_error);
    REQUIRE_THROWS_AS(ResamplerConstant<NumberData>("test", -1), std::runtime_error);
  }

  SECTION("Handles invalid message types") {
    auto resampler = ResamplerConstant<NumberData>("test", 5);
    REQUIRE_THROWS_AS(resampler.receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
  }
}

SCENARIO("ResamplerConstant downsampling without t0", "[ResamplerConstant]") {
  auto resampler = ResamplerConstant<NumberData>("test", 10);  // Grid: msg_time + 10, +20,...

  WHEN("Input frequency higher than grid") {
    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 10.0},   // Sets grid: 11, 21, 31,...
        {5, 20.0},   // No output
        {8, 30.0},   // No output
        {11, 40.0},  // Output: t=11 with value=40.0 (grid aligned)
        {15, 50.0},  // No output
        {21, 60.0},  // Output: t=21 with value=60.0 (grid aligned)
    };

    for (const auto& [time, value] : inputs) {
      resampler.receive_data(create_message<NumberData>(time, NumberData{value}), 0);
    }

    resampler.execute();
    const auto& output = resampler.get_output_queue(0);
    REQUIRE(output.size() == 2);

    const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
    REQUIRE(msg1->time == 11);
    REQUIRE(msg1->data.value == 40.0);

    const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
    REQUIRE(msg2->time == 21);
    REQUIRE(msg2->data.value == 60.0);
  }
}

SCENARIO("ResamplerConstant upsampling without t0", "[ResamplerConstant]") {
  auto resampler = ResamplerConstant<NumberData>("test", 5);  // Grid: msg_time + 5, +10,...

  WHEN("Input frequency lower than grid") {
    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 10.0},   // Sets grid: 6, 11, 16,...
        {15, 20.0},  // Output: t=6,11 with value=10.0 (fills gaps)
        {25, 30.0},  // Output: t=16,21 with value=20.0
    };

    for (const auto& [time, value] : inputs) {
      resampler.receive_data(create_message<NumberData>(time, NumberData{value}), 0);
    }

    resampler.execute();
    const auto& output = resampler.get_output_queue(0);
    REQUIRE(output.size() == 4);

    std::vector<std::pair<timestamp_t, double>> expected = {{6, 10.0}, {11, 10.0}, {16, 20.0}, {21, 20.0}};

    for (size_t i = 0; i < expected.size(); ++i) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }
}

SCENARIO("ResamplerConstant with fixed t0", "[ResamplerConstant]") {
  auto resampler = ResamplerConstant<NumberData>("test", 10, 5);  // Grid: 5,15,25,...

  WHEN("Processing mixed frequency input") {
    std::vector<std::pair<timestamp_t, double>> inputs = {
        {3, 10.0},   // Sets next emit to t=5
        {8, 20.0},   // Output: t=5 with value=10.0
        {16, 30.0},  // Output: t=15 with value=20.0
        {31, 40.0},  // Output: t=25 with value=30.0
    };

    for (const auto& [time, value] : inputs) {
      resampler.receive_data(create_message<NumberData>(time, NumberData{value}), 0);
    }

    resampler.execute();
    const auto& output = resampler.get_output_queue(0);
    REQUIRE(output.size() == 3);

    std::vector<std::pair<timestamp_t, double>> expected = {{5, 10.0}, {15, 20.0}, {25, 30.0}};

    for (size_t i = 0; i < expected.size(); ++i) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }
}

SCENARIO("ResamplerConstant with t0 after first message", "[ResamplerConstant]") {
  auto resampler = ResamplerConstant<NumberData>("test", 10, 50);  // Grid: 50,60,70,...

  WHEN("First messages arrive before t0") {
    std::vector<std::pair<timestamp_t, double>> inputs = {
        {3, 10.0},   // Wait for t0
        {25, 20.0},  // Still wait for t0
        {48, 30.0},  // Sets next emit to t=50
        {55, 40.0},  // Output: t=50 with value=30.0
        {65, 50.0},  // Output: t=60 with value=40.0
    };

    for (const auto& [time, value] : inputs) {
      resampler.receive_data(create_message<NumberData>(time, NumberData{value}), 0);
    }

    resampler.execute();

    const auto& output = resampler.get_output_queue(0);
    REQUIRE(output.size() == 2);

    std::vector<std::pair<timestamp_t, double>> expected = {
        {50, 30.0},  // First grid point uses value known at t=48
        {60, 40.0}   // Second grid point uses value known at t=55
    };

    for (size_t i = 0; i < expected.size(); ++i) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
      REQUIRE(msg->time == expected[i].first);
      REQUIRE(msg->data.value == expected[i].second);
    }
  }
}