#include <catch2/catch.hpp>

#include "rtbot/Collector.h"
#include "rtbot/std/FiniteImpulseResponse.h"

using namespace rtbot;

SCENARIO("FIR operator processes signals correctly", "[fir]") {
  GIVEN("A simple moving average FIR filter") {
    std::vector<double> coeffs = {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0};
    auto fir = std::make_shared<FiniteImpulseResponse>("fir1", coeffs);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    fir->connect(col, 0, 0);

    WHEN("Processing a sequence of values") {
      fir->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
      fir->receive_data(create_message<NumberData>(2, NumberData{6.0}), 0);
      fir->receive_data(create_message<NumberData>(3, NumberData{9.0}), 0);
      fir->execute();

      THEN("Output is correctly computed") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);
        auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(6.0));
      }
    }
  }

  GIVEN("A FIR filter with non-trivial state") {
    std::vector<double> coeffs = {0.2, 0.3, 0.5};
    auto fir = std::make_shared<FiniteImpulseResponse>("fir3", coeffs);
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    fir->connect(col, 0, 0);

    // Fill buffer with initial state
    fir->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    fir->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
    fir->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
    fir->execute();

    WHEN("State is serialized and restored") {
      auto state = fir->collect();
      auto restored = std::make_shared<FiniteImpulseResponse>("fir3", coeffs);
      auto rcol = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);
      restored->restore_data_from_json(state);

      THEN("Restored filter produces same output") {
        col->reset();
        fir->receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
        fir->execute();
        restored->receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
        restored->execute();

        const auto& orig_output = col->get_data_queue(0);
        const auto& rest_output = rcol->get_data_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());
        auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output[0].get());
        auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output[0].get());
        REQUIRE(orig_msg->time == rest_msg->time);
        REQUIRE(orig_msg->data.value == rest_msg->data.value);
      }
    }
  }
}
