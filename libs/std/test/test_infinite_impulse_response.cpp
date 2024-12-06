#include <catch2/catch.hpp>

#include "rtbot/std/InfiniteImpulseResponse.h"

using namespace rtbot;

SCENARIO("InfiniteImpulseResponse operator handles basic filtering", "[iir]") {
  GIVEN("A simple first-order IIR filter") {
    std::vector<double> b = {1.0};
    std::vector<double> a = {0.5};
    auto iir = std::make_unique<InfiniteImpulseResponse>("iir1", b, a);

    WHEN("Processing a sequence of inputs") {
      iir->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      iir->execute();

      THEN("First output is purely feed-forward") {
        const auto& output = iir->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->data.value == Approx(1.0));
      }

      AND_WHEN("Processing second input") {
        iir->receive_data(create_message<NumberData>(2, NumberData{1.0}), 0);
        iir->execute();

        THEN("Output includes feedback term") {
          const auto& output = iir->get_output_queue(0);
          REQUIRE(output.size() == 2);
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[1].get());
          REQUIRE(msg->data.value == Approx(0.5));  // 1.0 - 0.5*1.0
        }
      }
    }
  }

  GIVEN("A second-order IIR filter") {
    std::vector<double> b = {1.0, -0.5};
    std::vector<double> a = {0.2, 0.1};
    auto iir = std::make_unique<InfiniteImpulseResponse>("iir2", b, a);

    WHEN("Processing a sequence of inputs") {
      // First input just gets buffered
      iir->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      iir->execute();
      REQUIRE(iir->get_output_queue(0).empty());

      // Second input produces first output using only feed-forward terms
      iir->receive_data(create_message<NumberData>(2, NumberData{1.0}), 0);
      iir->execute();
      {
        const auto& output = iir->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 2);
        REQUIRE(msg->data.value == Approx(0.5));  // 1.0 - 0.5*1.0
      }

      // Third input includes first feedback term
      iir->clear_all_output_ports();
      iir->receive_data(create_message<NumberData>(3, NumberData{1.0}), 0);
      iir->execute();
      {
        const auto& output = iir->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(0.4));  // 1.0 - 0.5*1.0 - 0.2*0.5
      }

      // Fourth input includes both feedback terms
      iir->clear_all_output_ports();
      iir->receive_data(create_message<NumberData>(4, NumberData{1.0}), 0);
      iir->execute();
      {
        const auto& output = iir->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 4);
        REQUIRE(msg->data.value == Approx(0.37));  // 1.0 - 0.5*1.0 - 0.2*0.4 - 0.1*0.5
      }
    }
  }
}

SCENARIO("InfiniteImpulseResponse operator validates input", "[iir]") {
  SECTION("Empty coefficient vectors") {
    REQUIRE_THROWS_AS(InfiniteImpulseResponse("iir", std::vector<double>{}, std::vector<double>{0.5}),
                      std::runtime_error);

    REQUIRE_THROWS_AS(InfiniteImpulseResponse("iir", std::vector<double>{1.0}, std::vector<double>{}),
                      std::runtime_error);
  }

  SECTION("Invalid message type") {
    auto iir = InfiniteImpulseResponse("iir", std::vector<double>{1.0}, std::vector<double>{0.5});
    REQUIRE_THROWS_AS(iir.receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
  }
}

SCENARIO("InfiniteImpulseResponse operator maintains buffer size", "[iir]") {
  GIVEN("A first-order IIR filter") {
    std::vector<double> b = {1.0};
    std::vector<double> a = {0.5};
    auto iir = std::make_unique<InfiniteImpulseResponse>("iir1", b, a);

    WHEN("Processing many inputs") {
      for (int i = 0; i < 10; i++) {
        iir->receive_data(create_message<NumberData>(i, NumberData{1.0}), 0);
      }
      iir->execute();

      THEN("Output remains stable") {
        const auto& output = iir->get_output_queue(0);
        REQUIRE(output.size() == 10);
        // Check last few outputs converge to steady state
        auto* last = dynamic_cast<const Message<NumberData>*>(output.back().get());
        REQUIRE(last->data.value == Approx(0.666667).epsilon(0.001));
      }
    }
  }
}

SCENARIO("InfiniteImpulseResponse operator handles serialization", "[iir]") {
  GIVEN("A second-order IIR filter with processed data") {
    std::vector<double> b = {1.0, -0.5};
    std::vector<double> a = {0.2, 0.1};
    auto iir = std::make_unique<InfiniteImpulseResponse>("iir2", b, a);

    // Process enough data to fill buffers
    iir->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    iir->receive_data(create_message<NumberData>(2, NumberData{0.5}), 0);
    iir->receive_data(create_message<NumberData>(3, NumberData{0.8}), 0);
    iir->execute();

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = iir->collect();

      // Create new operator with same configuration
      auto restored = std::make_unique<InfiniteImpulseResponse>("iir2", b, a);

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      // Process new data on both operators
      iir->receive_data(create_message<NumberData>(4, NumberData{0.7}), 0);
      restored->receive_data(create_message<NumberData>(4, NumberData{0.7}), 0);
      iir->execute();
      restored->execute();

      THEN("Both operators produce identical outputs") {
        const auto& orig_output = iir->get_output_queue(0);
        const auto& rest_output = restored->get_output_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());

        auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[0].get());
        auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[0].get());

        REQUIRE(orig_msg->time == rest_msg->time);
        REQUIRE(orig_msg->data.value == Approx(rest_msg->data.value));
      }
    }
  }
}