#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include <vector>

#include "rtbot/finance/RelativeStrengthIndex.h"

using namespace rtbot;

SCENARIO("RelativeStrengthIndex operator computes correct RSI values", "[rsi]") {
  GIVEN("A RelativeStrengthIndex operator with period 14") {
    auto rsi = RelativeStrengthIndex("rsi", 14);

    // Test data based on typical RSI calculation examples
    std::vector<double> values = {
        54.8,  56.8,  57.85, 59.85, 60.57, 61.1,  62.17, 60.6,  62.35, 62.15,
        62.35, 61.45, 62.8,  61.37, 62.5,  62.57, 60.8,  59.37, 60.35, 62.35,
        62.17, 62.55, 64.55, 64.37, 65.3,  64.42, 62.9,  61.6,  62.05, 60.05,
        59.7,  60.9,  60.25, 58.27, 58.7,  57.72, 58.1,  58.2};

    // Expected RSI values (0 means no output expected)
    std::vector<double> expected_rsis = {
        0,        0,        0,        0,        0,        0,        0,        0,
        0,        0,        0,        0,        0,        0,        74.21384, 74.33552,
        65.87129, 59.93370, 62.43288, 66.96205, 66.18862, 67.05377, 71.22679, 70.36299,
        72.23644, 67.86486, 60.99822, 55.79821, 57.15964, 49.81579, 48.63810, 52.76154,
        50.40119, 43.95111, 45.57992, 42.54534, 44.09946, 44.52472};

    WHEN("Processing the input sequence") {
      for (size_t i = 0; i < values.size(); i++) {
        rsi.receive_data(create_message<NumberData>(i + 1, NumberData{values[i]}), 0);
        rsi.execute();

        const auto& output = rsi.get_output_queue(0);

        if (i < 14) {
          THEN("No output is produced for first 14 values at index " + std::to_string(i)) {
            REQUIRE(output.empty());
          }
        } else {
          THEN("Correct RSI is produced at index " + std::to_string(i)) {
            REQUIRE(output.size() == 1);

            const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
            REQUIRE(msg != nullptr);
            REQUIRE(std::abs(msg->data.value - expected_rsis[i]) <= 0.00001);
          }
        }

        rsi.clear_all_output_ports();
      }
    }
  }
}

SCENARIO("RelativeStrengthIndex operator handles edge cases", "[rsi]") {
  SECTION("Small period") {
    auto rsi = RelativeStrengthIndex("rsi", 2);

    // Send 3 values (period + 1 needed for first output)
    rsi.receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    rsi.execute();
    REQUIRE(rsi.get_output_queue(0).empty());

    rsi.receive_data(create_message<NumberData>(2, NumberData{12.0}), 0);
    rsi.execute();
    REQUIRE(rsi.get_output_queue(0).empty());

    rsi.receive_data(create_message<NumberData>(3, NumberData{14.0}), 0);
    rsi.execute();

    // Should have output now
    const auto& output = rsi.get_output_queue(0);
    REQUIRE(output.size() == 1);

    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    REQUIRE(msg != nullptr);
    // All gains, no losses -> RSI should be 100
    REQUIRE(msg->data.value == 100.0);
  }

  SECTION("All losses") {
    auto rsi = RelativeStrengthIndex("rsi", 2);

    // Send decreasing values
    rsi.receive_data(create_message<NumberData>(1, NumberData{20.0}), 0);
    rsi.execute();
    rsi.receive_data(create_message<NumberData>(2, NumberData{15.0}), 0);
    rsi.execute();
    rsi.receive_data(create_message<NumberData>(3, NumberData{10.0}), 0);
    rsi.execute();

    const auto& output = rsi.get_output_queue(0);
    REQUIRE(output.size() == 1);

    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    REQUIRE(msg != nullptr);
    // All losses, no gains -> RSI should be 0
    REQUIRE(msg->data.value == 0.0);
  }

  SECTION("No change in values") {
    auto rsi = RelativeStrengthIndex("rsi", 2);

    // Send same value multiple times
    rsi.receive_data(create_message<NumberData>(1, NumberData{50.0}), 0);
    rsi.execute();
    rsi.receive_data(create_message<NumberData>(2, NumberData{50.0}), 0);
    rsi.execute();
    rsi.receive_data(create_message<NumberData>(3, NumberData{50.0}), 0);
    rsi.execute();

    const auto& output = rsi.get_output_queue(0);
    REQUIRE(output.size() == 1);

    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    REQUIRE(msg != nullptr);
    // No gains, no losses, average_loss is 0 -> RSI should be 100
    REQUIRE(msg->data.value == 100.0);
  }
}

SCENARIO("RelativeStrengthIndex reset functionality", "[rsi]") {
  GIVEN("An RSI operator with some state") {
    auto rsi = RelativeStrengthIndex("rsi", 3);

    // Add some data
    rsi.receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
    rsi.execute();
    rsi.receive_data(create_message<NumberData>(2, NumberData{12.0}), 0);
    rsi.execute();

    WHEN("Reset is called") {
      rsi.reset();

      THEN("Operator behaves as if freshly constructed") {
        // Add same data again
        rsi.receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
        rsi.execute();

        // Should have no output yet (buffer not full)
        REQUIRE(rsi.get_output_queue(0).empty());
      }
    }
  }
}
