#include <catch2/catch.hpp>

#include "rtbot/std/ResamplerHermite.h"

using namespace rtbot;

SCENARIO("ResamplerHermite handles basic resampling", "[resampler][hermite]") {
  GIVEN("A hermite resampler with interval 5") {
    auto resampler = make_resampler_hermite("test", 5);

    WHEN("Receiving sequential messages") {
      // Feed 4 points to fill the buffer
      resampler->receive_data(create_message<NumberData>(0, NumberData{0.0}), 0);
      resampler->receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
      resampler->receive_data(create_message<NumberData>(4, NumberData{8.0}), 0);
      resampler->receive_data(create_message<NumberData>(6, NumberData{12.0}), 0);
      resampler->execute();

      THEN("Buffer is filled correctly") {
        REQUIRE(resampler->buffer_full());
        REQUIRE(resampler->buffer_size() == 4);
      }

      AND_THEN("Interpolation produces correct points") {
        const auto& output_queue = resampler->get_output_queue(0);
        REQUIRE(output_queue.size() == 1);  // Should emit for t=2

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output_queue[0].get());
        REQUIRE(msg->time == 2);
        REQUIRE(msg->data.value == Approx(4.0).margin(0.1));
      }
    }
  }
}

SCENARIO("ResamplerHermite handles custom start time", "[resampler][hermite]") {
  GIVEN("A hermite resampler with interval 5 and t0=10") {
    auto resampler = make_resampler_hermite("test", 5, 10);

    WHEN("Receiving messages starting before t0") {
      resampler->receive_data(create_message<NumberData>(8, NumberData{16.0}), 0);
      resampler->receive_data(create_message<NumberData>(9, NumberData{18.0}), 0);
      resampler->receive_data(create_message<NumberData>(11, NumberData{22.0}), 0);
      resampler->receive_data(create_message<NumberData>(12, NumberData{24.0}), 0);
      resampler->execute();

      THEN("First emission is at t0") {
        const auto& output_queue = resampler->get_output_queue(0);
        REQUIRE(!output_queue.empty());
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output_queue[0].get());
        REQUIRE(msg->time == 10);
      }
    }
  }
}

SCENARIO("ResamplerHermite handles various data patterns", "[resampler][hermite]") {
  GIVEN("A hermite resampler with interval 2") {
    auto resampler = make_resampler_hermite("test", 2);

    WHEN("Receiving linear sequence") {
      for (int i = 0; i < 8; i += 2) {
        resampler->receive_data(create_message<NumberData>(i, NumberData{static_cast<double>(i)}), 0);
        resampler->execute();
      }

      THEN("Interpolation follows linear pattern") {
        const auto& output_queue = resampler->get_output_queue(0);
        REQUIRE(output_queue.size() >= 2);

        for (size_t i = 0; i < output_queue.size(); i++) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output_queue[i].get());
          REQUIRE(msg->data.value == Approx(msg->time).margin(0.1));
        }
      }
    }

    WHEN("Receiving quadratic sequence") {
      std::vector<std::pair<timestamp_t, double>> points = {{0, 0}, {2, 4}, {4, 16}, {6, 36}, {8, 64}};

      for (const auto& [t, v] : points) {
        resampler->receive_data(create_message<NumberData>(t, NumberData{v}), 0);
        resampler->execute();
      }

      THEN("Interpolation follows quadratic curve") {
        const auto& output_queue = resampler->get_output_queue(0);
        REQUIRE(!output_queue.empty());

        // Verify interpolated points follow quadratic pattern approximately
        for (const auto& msg_ptr : output_queue) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(msg_ptr.get());
          double expected = msg->time * msg->time;                   // quadratic function
          REQUIRE(msg->data.value == Approx(expected).margin(2.0));  // Larger margin due to interpolation
        }
      }
    }
  }
}

SCENARIO("ResamplerHermite handles edge cases", "[resampler][hermite]") {
  SECTION("Invalid interval") {
    REQUIRE_THROWS_AS(make_resampler_hermite("test", 0), std::runtime_error);
    REQUIRE_THROWS_AS(make_resampler_hermite("test", -1), std::runtime_error);
  }

  GIVEN("A hermite resampler with interval 3") {
    auto resampler = make_resampler_hermite("test", 3);

    WHEN("Receiving sparse data") {
      resampler->receive_data(create_message<NumberData>(0, NumberData{0.0}), 0);
      resampler->receive_data(create_message<NumberData>(10, NumberData{10.0}), 0);
      resampler->receive_data(create_message<NumberData>(20, NumberData{20.0}), 0);
      resampler->receive_data(create_message<NumberData>(30, NumberData{30.0}), 0);
      resampler->execute();

      THEN("Interpolation still produces valid points") {
        const auto& output_queue = resampler->get_output_queue(0);
        REQUIRE(!output_queue.empty());

        for (const auto& msg_ptr : output_queue) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(msg_ptr.get());
          // Check timestamps are multiples of interval
          REQUIRE((msg->time - 1) % 3 == 0);
          // Check values are within reasonable bounds
          REQUIRE(msg->data.value >= 0.0);
          REQUIRE(msg->data.value <= 30.0);
        }
      }
    }
  }
}

SCENARIO("ResamplerHermite maintains state correctly", "[resampler][hermite][State]") {
  GIVEN("A hermite resampler with interval 4") {
    auto resampler = make_resampler_hermite("test", 4);

    WHEN("State is serialized and restored") {
      // Add some data
      resampler->receive_data(create_message<NumberData>(0, NumberData{0.0}), 0);
      resampler->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      resampler->receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
      resampler->receive_data(create_message<NumberData>(6, NumberData{6.0}), 0);
      resampler->execute();

      // Serialize state
      Bytes state = resampler->collect();

      // Create new resampler and restore state
      auto restored = make_resampler_hermite("test", 4);
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Restored resampler maintains same properties") {
        REQUIRE(restored->get_interval() == resampler->get_interval());
        REQUIRE(restored->get_next_emission_time() == resampler->get_next_emission_time());
        REQUIRE(restored->buffer_size() == resampler->buffer_size());
        REQUIRE(*restored == *resampler);
      }

      AND_WHEN("New data is processed") {
        restored->receive_data(create_message<NumberData>(8, NumberData{8.0}), 0);
        resampler->receive_data(create_message<NumberData>(8, NumberData{8.0}), 0);

        restored->execute();
        resampler->execute();

        THEN("Both produce identical output") {
          const auto& orig_output = resampler->get_output_queue(0);
          const auto& rest_output = restored->get_output_queue(0);

          REQUIRE(orig_output.size() == rest_output.size());

          for (size_t i = 0; i < orig_output.size(); i++) {
            const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[i].get());
            const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[i].get());

            REQUIRE(orig_msg->time == rest_msg->time);
            REQUIRE(orig_msg->data.value == Approx(rest_msg->data.value).margin(0.000001));
          }
        }
      }
    }
  }
}

SCENARIO("ResamplerHermite handles non-uniform input times", "[resampler][hermite]") {
  GIVEN("A hermite resampler with interval 5") {
    auto resampler = make_resampler_hermite("test", 5, 0);

    WHEN("Receiving irregularly spaced timestamps") {
      resampler->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      resampler->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      resampler->receive_data(create_message<NumberData>(7, NumberData{7.0}), 0);
      resampler->receive_data(create_message<NumberData>(11, NumberData{11.0}), 0);
      resampler->receive_data(create_message<NumberData>(12, NumberData{12.0}), 0);
      resampler->execute();

      THEN("Output times are properly aligned to interval") {
        const auto& output_queue = resampler->get_output_queue(0);
        for (const auto& msg_ptr : output_queue) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(msg_ptr.get());
          // Verify timestamps align with interval
          REQUIRE(msg->time % 5 == 0);
        }
      }
    }
  }
}

SCENARIO("ResamplerHermite correctly interpolates a parabola over extended interval", "[resampler][hermite]") {
  GIVEN("A resampler configured for intervals of 5 time units") {
    auto resampler = make_resampler_hermite("test_resampler", 5, 5);

    WHEN("Fed integer-time points from the parabola y = (x/10)^2") {
      // Feed points with enough spacing to test interpolation
      resampler->receive_data(create_message<NumberData>(0, NumberData{0.0}), 0);
      resampler->receive_data(create_message<NumberData>(10, NumberData{1.0}), 0);
      resampler->receive_data(create_message<NumberData>(20, NumberData{4.0}), 0);
      resampler->receive_data(create_message<NumberData>(30, NumberData{9.0}), 0);
      resampler->execute();

      THEN("It should emit values matching y = (x/10)^2 at 5-unit intervals") {
        const auto& output = resampler->get_output_queue(0);
        REQUIRE(output.size() == 3);  // Should emit values at t = 10,15,20

        // Check interpolated values
        std::vector<std::pair<timestamp_t, double>> expected = {
            {10, 1.0},   // (10/10)^2 = 1.0
            {15, 2.25},  // (15/10)^2 = 2.25
            {20, 4.0},   // (20/10)^2 = 4.0
        };

        for (size_t i = 0; i < expected.size(); i++) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
          REQUIRE(msg->time == expected[i].first);
          REQUIRE(msg->data.value == Approx(expected[i].second).margin(0.01));
        }
      }
    }
  }
}

SCENARIO("ResamplerHermite correctly interpolates a complex oscillating function", "[resampler][hermite]") {
  GIVEN("A resampler configured for 100-unit intervals") {
    auto resampler = make_resampler_hermite("test_resampler", 100, 100);

    WHEN("Fed points from y = sin(x/200) + (x/1000)^2") {
      // Helper function to calculate expected values
      auto func = [](timestamp_t t) -> double { return std::sin(t / 200.0) + std::pow(t / 1000.0, 2); };

      // Feed points with 200-unit spacing from 0 to 1000
      std::vector<timestamp_t> input_times = {0, 200, 400, 600, 800, 1000};
      for (auto t : input_times) {
        resampler->receive_data(create_message<NumberData>(t, NumberData{func(t)}), 0);
      }
      resampler->execute();

      THEN("It should emit accurate interpolated values at 100-unit intervals") {
        const auto& output = resampler->get_output_queue(0);

        // Should emit values at t = 200,300,...,800
        REQUIRE(output.size() == 7);

        // Check each interpolated value
        for (size_t i = 0; i < output.size(); i++) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[i].get());
          timestamp_t expected_time = 100 * (i + 2);
          double expected_value = func(expected_time);

          INFO("At time = " << expected_time);
          REQUIRE(msg->time == expected_time);
          REQUIRE(msg->data.value == Approx(expected_value).margin(0.05));
        }

        AND_THEN("The interpolation preserves important signal characteristics") {
          // Test monotonicity around local extrema
          for (size_t i = 1; i < output.size() - 1; i++) {
            const auto* prev = dynamic_cast<const Message<NumberData>*>(output[i - 1].get());
            const auto* curr = dynamic_cast<const Message<NumberData>*>(output[i].get());
            const auto* next = dynamic_cast<const Message<NumberData>*>(output[i + 1].get());

            // If original function has a local max/min, interpolation should preserve it
            if (func(curr->time - 1) < func(curr->time) && func(curr->time) > func(curr->time + 1)) {
              REQUIRE(prev->data.value < curr->data.value);
              REQUIRE(curr->data.value > next->data.value);
            }
          }
        }
      }
    }
  }
}