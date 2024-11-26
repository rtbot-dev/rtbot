#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>

#include "rtbot/std/MovingAverage.h"

using namespace rtbot;

SCENARIO("MovingAverage operator handles basic calculations", "[moving_average]") {
  GIVEN("A MovingAverage operator with window size 3") {
    auto ma = MovingAverage("test_ma", 3);

    WHEN("Receiving less messages than window size") {
      ma.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      ma.execute();

      THEN("No output is produced") {
        const auto& output = ma.get_output_queue(0);
        REQUIRE(output.empty());
        REQUIRE(ma.mean() == 2.0);
      }

      AND_WHEN("Second message arrives") {
        ma.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
        ma.execute();

        THEN("Still no output but average is updated") {
          const auto& output = ma.get_output_queue(0);
          REQUIRE(output.empty());
          REQUIRE(ma.mean() == 3.0);  // (2 + 4) / 2
        }
      }
    }

    WHEN("Buffer becomes full") {
      // Fill buffer with values [2.0, 4.0, 6.0]
      ma.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      ma.execute();
      ma.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
      ma.execute();
      ma.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
      ma.execute();

      THEN("Output is produced with correct average") {
        const auto& output = ma.get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == 4.0);  // (2 + 4 + 6) / 3
      }

      AND_WHEN("New value arrives") {
        ma.receive_data(create_message<NumberData>(4, NumberData{8.0}), 0);
        ma.execute();

        THEN("Window slides and new average is calculated") {
          const auto& output = ma.get_output_queue(0);
          REQUIRE(output.size() == 1);

          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == 4);
          REQUIRE(msg->data.value == 6.0);  // (4 + 6 + 8) / 3
        }
      }
    }
  }
}

SCENARIO("MovingAverage operator handles state serialization", "[moving_average]") {
  GIVEN("A MovingAverage operator with some data") {
    auto ma = MovingAverage("test_ma", 3);

    // Add some data
    ma.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
    ma.execute();
    ma.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
    ma.execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = ma.collect();

      // Create new operator
      auto restored = MovingAverage("test_ma", 3);

      // Restore state
      Bytes::const_iterator it = state.cbegin();
      restored.restore(it);

      THEN("State is correctly preserved") {
        REQUIRE(restored.mean() == ma.mean());

        AND_WHEN("New data is added to both") {
          ma.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
          restored.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);

          ma.execute();
          restored.execute();

          THEN("Both produce identical output") {
            const auto& orig_output = ma.get_output_queue(0);
            const auto& rest_output = restored.get_output_queue(0);

            REQUIRE(orig_output.size() == rest_output.size());

            if (!orig_output.empty()) {
              const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output.front().get());
              const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output.front().get());

              REQUIRE(orig_msg->time == rest_msg->time);
              REQUIRE(orig_msg->data.value == rest_msg->data.value);
            }
          }
        }
      }
    }
  }
}

SCENARIO("MovingAverage operator handles edge cases", "[moving_average]") {
  SECTION("Invalid window size") { REQUIRE_THROWS_AS(MovingAverage("test_ma", 0), std::runtime_error); }

  SECTION("Window size of 1") {
    auto ma = MovingAverage("test_ma", 1);

    ma.receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
    ma.execute();

    const auto& output = ma.get_output_queue(0);
    REQUIRE(output.size() == 1);

    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    REQUIRE(msg->data.value == 42.0);
  }

  SECTION("Large numbers") {
    auto ma = MovingAverage("test_ma", 3);
    double large_value = 1e15;

    ma.receive_data(create_message<NumberData>(1, NumberData{large_value}), 0);
    ma.execute();
    ma.receive_data(create_message<NumberData>(2, NumberData{large_value + 3}), 0);
    ma.execute();
    ma.receive_data(create_message<NumberData>(3, NumberData{large_value + 6}), 0);
    ma.execute();

    const auto& output = ma.get_output_queue(0);
    REQUIRE(output.size() == 1);

    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    REQUIRE(msg->data.value == large_value + 3);  // Average of the three values
  }

  SECTION("Numerical stability") {
    auto ma = MovingAverage("test_ma", 3);

    // Add sequence of very small and very large numbers
    ma.receive_data(create_message<NumberData>(1, NumberData{1e-10}), 0);
    ma.execute();
    ma.receive_data(create_message<NumberData>(2, NumberData{1e10}), 0);
    ma.execute();
    ma.receive_data(create_message<NumberData>(3, NumberData{1e-10}), 0);
    ma.execute();

    const auto& output = ma.get_output_queue(0);
    REQUIRE(output.size() == 1);

    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    double expected = (1e-10 + 1e10 + 1e-10) / 3.0;
    REQUIRE(msg->data.value == expected);
  }
}