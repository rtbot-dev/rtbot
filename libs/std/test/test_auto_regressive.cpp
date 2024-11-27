#include <catch2/catch.hpp>

#include "rtbot/std/AutoRegressive.h"

using namespace rtbot;

SCENARIO("AutoRegressive operator processes initial messages correctly", "[AutoRegressive]") {
  GIVEN("An AR(2) operator with coefficients [0.5, 0.3]") {
    std::vector<double> coeff{0.5, 0.3};
    auto ar = std::make_unique<AutoRegressive>("ar1", coeff);

    WHEN("First message arrives") {
      ar->receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      ar->execute();

      THEN("No output is produced") { REQUIRE(ar->get_output_queue(0).empty()); }
    }

    WHEN("Second message arrives") {
      ar->receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
      ar->execute();
      ar->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
      ar->execute();

      THEN("First output is produced") {
        const auto& output = ar->get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 2);
        // Buffer is [2.0, 3.0]
        // Output = 0.5 * 3.0 + 0.3 * 2.0 = 2.1
        REQUIRE(msg->data.value == Approx(2.1));
      }
    }
  }
}

SCENARIO("AutoRegressive operator handles continuous message flow", "[AutoRegressive]") {
  GIVEN("An AR(2) operator with filled buffer") {
    std::vector<double> coeff{0.5, 0.3};
    auto ar = std::make_unique<AutoRegressive>("ar1", coeff);

    // Fill buffer
    ar->receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
    ar->execute();
    ar->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
    ar->execute();

    WHEN("Next message arrives") {
      ar->receive_data(create_message<NumberData>(3, NumberData{4.0}), 0);
      ar->execute();

      THEN("Output is calculated correctly") {
        const auto& output = ar->get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 3);
        // Buffer is [3.0, 4.0]
        // Output = 0.5 * 4.0 + 0.3 * 3.0 = 2.9
        REQUIRE(msg->data.value == Approx(2.9));
      }
    }
  }
}

SCENARIO("AutoRegressive operator handles configuration", "[AutoRegressive]") {
  SECTION("Empty coefficients") { REQUIRE_THROWS_AS(AutoRegressive("ar1", std::vector<double>{}), std::runtime_error); }

  SECTION("Coefficient access") {
    std::vector<double> coeff{0.5, -0.2, 0.1};
    auto ar = std::make_unique<AutoRegressive>("ar1", coeff);
    REQUIRE(ar->get_coefficients() == coeff);
  }
}

SCENARIO("AutoRegressive operator with three coefficients", "[AutoRegressive]") {
  GIVEN("An AR(3) operator") {
    std::vector<double> coeff{0.5, -0.2, 0.1};
    auto ar = std::make_unique<AutoRegressive>("ar1", coeff);

    WHEN("Buffer fills") {
      ar->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      ar->execute();
      REQUIRE(ar->get_output_queue(0).empty());

      ar->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      ar->execute();
      REQUIRE(ar->get_output_queue(0).empty());

      ar->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      ar->execute();

      THEN("First output is calculated correctly") {
        const auto& output = ar->get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 3);
        // Buffer is [1.0, 2.0, 3.0]
        // Output = 0.5 * 3.0 + (-0.2) * 2.0 + 0.1 * 1.0 = 1.2
        REQUIRE(msg->data.value == Approx(1.2));
      }

      AND_WHEN("Another message arrives") {
        ar->receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
        ar->execute();

        THEN("New output is calculated correctly") {
          const auto& output = ar->get_output_queue(0);
          REQUIRE(output.size() == 1);

          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == 4);
          // Buffer is [2.0, 3.0, 4.0]
          // Output = 0.5 * 4.0 + (-0.2) * 3.0 + 0.1 * 2.0 = 1.6
          REQUIRE(msg->data.value == Approx(1.6));
        }
      }
    }
  }
}