#include <catch2/catch.hpp>
#include <cmath>

#include "rtbot/std/StandardDeviation.h"

using namespace rtbot;

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

        // Expected std dev for [2,4,6,8] = sqrt(10/3) ≈ 2.582
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