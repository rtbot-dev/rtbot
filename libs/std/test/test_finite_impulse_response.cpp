#include <catch2/catch.hpp>

#include "rtbot/std/FiniteImpulseResponse.h"

using namespace rtbot;

SCENARIO("FIR operator processes signals correctly", "[fir]") {
  GIVEN("A simple moving average FIR filter") {
    std::vector<double> coeffs = {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0};
    auto fir = make_fir("fir1", coeffs);

    WHEN("Processing a sequence of values") {
      fir->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
      fir->receive_data(create_message<NumberData>(2, NumberData{6.0}), 0);
      fir->receive_data(create_message<NumberData>(3, NumberData{9.0}), 0);
      fir->execute();

      THEN("Output is correctly computed") {
        const auto& output = fir->get_output_queue(0);
        REQUIRE(output.size() == 1);
        auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(6.0));
      }
    }
  }

  GIVEN("A FIR filter with non-trivial state") {
    std::vector<double> coeffs = {0.2, 0.3, 0.5};
    auto fir = make_fir("fir3", coeffs);

    // Fill buffer with initial state
    fir->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    fir->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    fir->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
    fir->execute();

    WHEN("State is serialized and restored") {
      Bytes state = fir->collect();
      auto restored = make_fir("fir3", coeffs);
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Restored filter produces same output") {
        fir->receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
        fir->execute();
        restored->receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
        restored->execute();

        const auto& orig_output = fir->get_output_queue(0);
        const auto& rest_output = restored->get_output_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());
        auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[0].get());
        auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[0].get());
        REQUIRE(orig_msg->time == rest_msg->time);
        REQUIRE(orig_msg->data.value == rest_msg->data.value);
      }
    }
  }
}