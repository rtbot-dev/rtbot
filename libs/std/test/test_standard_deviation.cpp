#include <catch2/catch.hpp>
#include <cmath>

#include "rtbot/std/StandardDeviation.h"

using namespace rtbot;
// Helper function to calculate rolling standard deviation for validation
double calculate_rolling_std(const std::vector<double>& values, size_t window_size) {
  if (values.size() < window_size) return 0.0;

  // Get window of values
  auto start = values.end() - window_size;
  auto end = values.end();

  // Calculate mean
  double sum = std::accumulate(start, end, 0.0);
  double mean = sum / window_size;

  // Calculate sum of squared differences
  double sq_sum = std::accumulate(start, end, 0.0, [mean](double acc, double val) {
    double diff = val - mean;
    return acc + (diff * diff);
  });

  // Return population standard deviation
  return std::sqrt(sq_sum / window_size);
}

SCENARIO("StandardDeviation operator handles basic calculations", "[StandardDeviation]") {
  GIVEN("A StandardDeviation operator with window size 4") {
    auto sd = StandardDeviation("sd1", 4);

    WHEN("Receiving a sequence of messages") {
      sd.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      sd.execute();
      sd.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
      sd.execute();
      sd.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
      sd.execute();
      sd.receive_data(create_message<NumberData>(4, NumberData{8.0}), 0);
      sd.execute();

      THEN("Standard deviation is calculated correctly") {
        const auto& output = sd.get_output_queue(0);
        REQUIRE(output.size() == 1);

        auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 4);

        // Expected std dev for [2,4,6,8] = sqrt((3*3 + 1 + 1 + 3*3)/3) = sqrt(20/3)≈ 2.582
        REQUIRE(msg->data.value == Approx(2.582).epsilon(0.001));
      }
    }
  }
}

SCENARIO("StandardDeviation operator handles edge cases", "[StandardDeviation]") {
  SECTION("Invalid window size") { REQUIRE_THROWS_AS(StandardDeviation("sd1", 0), std::runtime_error); }

  GIVEN("A StandardDeviation operator with window size 1") {
    auto sd = StandardDeviation("sd1", 1);

    WHEN("Receiving a single message") {
      sd.receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      sd.execute();

      THEN("Standard deviation is zero") {
        const auto& output = sd.get_output_queue(0);
        REQUIRE(output.size() == 1);

        auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->data.value == 0.0);
      }
    }
  }

  GIVEN("A StandardDeviation operator with window size 2") {
    auto sd = StandardDeviation("sd1", 2);

    WHEN("All values are the same") {
      sd.receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
      sd.execute();
      sd.receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
      sd.execute();

      THEN("Standard deviation is zero") {
        const auto& output = sd.get_output_queue(0);
        REQUIRE(output.size() == 1);

        auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->data.value == 0.0);
      }
    }
  }
}

SCENARIO("StandardDeviation operator handles state serialization", "[StandardDeviation]") {
  GIVEN("A StandardDeviation operator with processed messages") {
    auto sd = StandardDeviation("sd1", 3);

    // Add some data with gaps in timestamps
    sd.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
    sd.execute();
    sd.receive_data(create_message<NumberData>(3, NumberData{4.0}), 0);
    sd.execute();
    sd.receive_data(create_message<NumberData>(5, NumberData{6.0}), 0);
    sd.execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = sd.collect();

      // Create new operator
      auto restored = StandardDeviation("sd1", 3);

      // Restore state
      auto it = state.cbegin();
      restored.restore(it);

      THEN("Statistical calculations match") {
        const auto& orig_output = sd.get_output_queue(0);
        const auto& rest_output = restored.get_output_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());

        if (!orig_output.empty()) {
          auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[0].get());
          auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[0].get());

          REQUIRE(orig_msg->time == rest_msg->time);
          REQUIRE(orig_msg->data.value == rest_msg->data.value);
        }
      }

      AND_WHEN("New messages are processed") {
        restored.receive_data(create_message<NumberData>(7, NumberData{8.0}), 0);
        restored.execute();

        THEN("Calculations continue correctly") {
          const auto& output = restored.get_output_queue(0);
          REQUIRE(output.size() == 2);

          auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == 5);

          // Expected std dev for [4,6,8] ≈ 2.0
          REQUIRE(msg->data.value == Approx(2.0).epsilon(0.001));
        }
      }
    }
  }
}

SCENARIO("StandardDeviation operator handles long data streams correctly", "[StandardDeviation]") {
  GIVEN("A StandardDeviation operator with window size 3") {
    auto sd = StandardDeviation("sd1", 3);

    WHEN("Processing a stream of increasing numbers") {
      // This will create a stream: 1,2,3,4,5,6,7,8,9,10
      // For window size 3, we expect these windows:
      // [1,2,3] -> std = sqrt((2/2)) = 1
      // [2,3,4] -> std = sqrt((2/2)) = 1
      // [3,4,5] -> std = sqrt((2/2)) = 1
      // And so on...

      std::vector<double> expected_stds;
      for (int i = 1; i <= 8; i++) {   // We'll get 8 standard deviations for 10 numbers
        expected_stds.push_back(1.0);  // Each window has a std dev of 1.0
      }

      // Process messages one by one
      std::vector<double> actual_stds;
      for (int i = 1; i <= 10; i++) {
        sd.receive_data(create_message<NumberData>(i, NumberData{static_cast<double>(i)}), 0);
        sd.execute();

        // Check output once we have enough data
        const auto& output = sd.get_output_queue(0);
        if (!output.empty()) {
          auto* msg = dynamic_cast<const Message<NumberData>*>(output.back().get());
          REQUIRE(msg != nullptr);
          actual_stds.push_back(msg->data.value);
        }
        sd.clear_all_output_ports();
      }

      THEN("All standard deviations are computed correctly") {
        REQUIRE(actual_stds.size() == expected_stds.size());
        for (size_t i = 0; i < actual_stds.size(); i++) {
          REQUIRE(actual_stds[i] == Approx(expected_stds[i]).epsilon(0.001));
        }
      }
    }

    WHEN("Processing a constant stream") {
      // All numbers are 5.0 - standard deviation should always be 0
      for (int i = 1; i <= 10; i++) {
        sd.receive_data(create_message<NumberData>(i, NumberData{5.0}), 0);
        sd.execute();

        const auto& output = sd.get_output_queue(0);
        if (!output.empty()) {
          auto* msg = dynamic_cast<const Message<NumberData>*>(output.back().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->data.value == Approx(0.0).margin(1e-10));
        }
        sd.clear_all_output_ports();
      }
    }

    WHEN("Processing numbers with known standard deviation") {
      // Using values: 2, 4, 4, 4, 5, 5, 7, 9
      // Each window of 3 will have a different std dev
      std::vector<double> values = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};

      // Calculate expected standard deviations for each window of 3
      std::vector<double> expected_stds;
      for (size_t i = 0; i <= values.size() - 3; i++) {
        double mean = (values[i] + values[i + 1] + values[i + 2]) / 3.0;
        double sum_sq_diff = pow(values[i] - mean, 2) + pow(values[i + 1] - mean, 2) + pow(values[i + 2] - mean, 2);
        double std_dev = sqrt(sum_sq_diff / 2.0);  // Using n-1 = 2 for sample std dev
        expected_stds.push_back(std_dev);
      }

      std::vector<double> actual_stds;
      for (size_t i = 0; i < values.size(); i++) {
        sd.receive_data(create_message<NumberData>(i + 1, NumberData{values[i]}), 0);
        sd.execute();

        const auto& output = sd.get_output_queue(0);
        if (!output.empty()) {
          auto* msg = dynamic_cast<const Message<NumberData>*>(output.back().get());
          REQUIRE(msg != nullptr);
          actual_stds.push_back(msg->data.value);
        }
        sd.clear_all_output_ports();
      }

      THEN("All standard deviations match expected values") {
        REQUIRE(actual_stds.size() == expected_stds.size());
        for (size_t i = 0; i < actual_stds.size(); i++) {
          REQUIRE(actual_stds[i] == Approx(expected_stds[i]).epsilon(0.001));
        }
      }
    }
  }
}