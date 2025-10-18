#include <catch2/catch.hpp>
#include <cmath>

#include "rtbot/std/MovingSum.h"

using namespace rtbot;

SCENARIO("MovingSum operator handles basic calculations", "[moving_sum]") {
  GIVEN("A MovingSum operator with window size 3") {
    auto ms = MovingSum("test_ms", 3);

    WHEN("Receiving less messages than window size") {
      ms.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      ms.execute();

      THEN("No output is produced") {
        const auto& output = ms.get_output_queue(0);
        REQUIRE(output.empty());
        REQUIRE(ms.sum() == 2.0);
      }

      AND_WHEN("Second message arrives") {
        ms.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
        ms.execute();

        THEN("Still no output but average is updated") {
          const auto& output = ms.get_output_queue(0);
          REQUIRE(output.empty());
          REQUIRE(ms.sum() == 6.0);  // (2 + 4)
        }
      }
    }

    WHEN("Buffer becomes full") {
      // Fill buffer with values [2.0, 4.0, 6.0]
      ms.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      ms.execute();
      ms.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
      ms.execute();
      ms.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
      ms.execute();

      THEN("Output is produced with correct average") {
        const auto& output = ms.get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == 12.0);  // (2 + 4 + 6)
      }

      AND_WHEN("New value arrives") {
        ms.clear_all_output_ports();
        ms.receive_data(create_message<NumberData>(4, NumberData{8.0}), 0);
        ms.execute();

        THEN("Window slides and new average is calculated") {
          const auto& output = ms.get_output_queue(0);
          REQUIRE(output.size() == 1);

          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == 4);
          REQUIRE(msg->data.value == 18);  // (4 + 6 + 8)
        }
      }
    }
  }
}

SCENARIO("MovingSum operator handles state serialization", "[moving_sum]") {
  GIVEN("A MovingSum operator with some data") {
    auto ms = MovingSum("test_ms", 3);

    // Add some data
    ms.receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
    ms.execute();
    ms.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
    ms.execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = ms.collect();

      // Create new operator
      auto restored = MovingSum("test_ms", 3);

      // Restore state
      Bytes::const_iterator it = state.cbegin();
      restored.restore(it);

      THEN("State is correctly preserved") {
        REQUIRE(restored.sum() == ms.sum());

        AND_WHEN("New data is added to both") {
          ms.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
          restored.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);

          ms.execute();
          restored.execute();

          THEN("Both produce identical output") {
            const auto& orig_output = ms.get_output_queue(0);
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

SCENARIO("MovingSum operator handles edge cases", "[moving_sum]") {
  SECTION("Invalid window size") { REQUIRE_THROWS_AS(MovingSum("test_ms", 0), std::runtime_error); }

  SECTION("Window size of 1") {
    auto ms = MovingSum("test_ms", 1);

    ms.receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
    ms.execute();

    const auto& output = ms.get_output_queue(0);
    REQUIRE(output.size() == 1);

    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    REQUIRE(msg->data.value == 42.0);
  }

  SECTION("Large numbers") {
    auto ms = MovingSum("test_ms", 3);
    double large_value = 1e15;

    ms.receive_data(create_message<NumberData>(1, NumberData{large_value}), 0);
    ms.execute();
    ms.receive_data(create_message<NumberData>(2, NumberData{large_value + 3}), 0);
    ms.execute();
    ms.receive_data(create_message<NumberData>(3, NumberData{large_value + 6}), 0);
    ms.execute();

    const auto& output = ms.get_output_queue(0);
    REQUIRE(output.size() == 1);

    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    REQUIRE(msg->data.value == 3 * large_value + 9);  // Sum of the three values
  }

  SECTION("Numerical stability") {
    auto ms = MovingSum("test_ms", 3);

    // Add sequence of very small and very large numbers
    ms.receive_data(create_message<NumberData>(1, NumberData{1e-10}), 0);
    ms.execute();
    ms.receive_data(create_message<NumberData>(2, NumberData{1e10}), 0);
    ms.execute();
    ms.receive_data(create_message<NumberData>(3, NumberData{1e-10}), 0);
    ms.execute();

    const auto& output = ms.get_output_queue(0);
    REQUIRE(output.size() == 1);

    const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
    double expected = (1e-10 + 1e10 + 1e-10);
    REQUIRE(msg->data.value == expected);
  }
}