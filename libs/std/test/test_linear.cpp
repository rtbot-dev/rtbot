#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/std/Linear.h"

using namespace rtbot;

SCENARIO("Linear operator handles basic operations", "[linear]") {
  GIVEN("A Linear operator with two coefficients") {
    std::vector<double> coeffs = {2.0, -1.0};  // 2x - y
    auto linear = std::make_shared<Linear>("linear1", coeffs);

    WHEN("Receiving synchronized messages") {
      linear->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);  // x = 3
      linear->receive_data(create_message<NumberData>(1, NumberData{1.0}), 1);  // y = 1
      linear->execute();

      THEN("Linear combination is calculated correctly") {
        const auto& output = linear->get_output_queue(0);
        REQUIRE(!output.empty());
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == Approx(5.0));  // 2*3 - 1 = 5
      }
    }

    WHEN("Receiving unsynchronized messages") {
      linear->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0);
      linear->receive_data(create_message<NumberData>(2, NumberData{1.0}), 1);
      linear->execute();

      THEN("No output is produced") {
        const auto& output = linear->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }
  }

  GIVEN("A Linear operator with three coefficients") {
    std::vector<double> coeffs = {1.0, 2.0, 3.0};  // x + 2y + 3z
    auto linear = std::make_shared<Linear>("linear2", coeffs);

    WHEN("Processing multiple sets of synchronized messages") {
      // First set
      linear->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      linear->receive_data(create_message<NumberData>(1, NumberData{2.0}), 1);
      linear->receive_data(create_message<NumberData>(1, NumberData{3.0}), 2);

      // Second set
      linear->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      linear->receive_data(create_message<NumberData>(2, NumberData{3.0}), 1);
      linear->receive_data(create_message<NumberData>(2, NumberData{4.0}), 2);

      linear->execute();

      THEN("All combinations are calculated correctly") {
        const auto& output = linear->get_output_queue(0);
        REQUIRE(output.size() == 2);

        // First set: 1 + 2*2 + 3*3 = 14
        const auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1 != nullptr);
        REQUIRE(msg1->time == 1);
        REQUIRE(msg1->data.value == Approx(14.0));

        // Second set: 2 + 2*3 + 3*4 = 20
        const auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2 != nullptr);
        REQUIRE(msg2->time == 2);
        REQUIRE(msg2->data.value == Approx(20.0));
      }
    }
  }
}

SCENARIO("Linear operator validates configuration", "[linear]") {
  SECTION("Minimum number of coefficients") {
    REQUIRE_THROWS_AS(Linear("linear", std::vector<double>{1.0}), std::runtime_error);
  }

  SECTION("Coefficient count matches port count") {
    std::vector<double> coeffs = {1.0, 2.0, 3.0};
    auto linear = std::make_shared<Linear>("linear", coeffs);
    REQUIRE(linear->num_data_ports() == 3);
    REQUIRE(linear->get_coefficients().size() == 3);
  }
}

SCENARIO("Linear operator handles serialization", "[linear][serialization]") {
  GIVEN("A linear join" ) {
    auto linear = std::make_shared<Linear>("linear", std::vector<double>{1.0, 2.0, 3.0});
    linear->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
    linear->receive_data(create_message<NumberData>(1, NumberData{1.0}), 1);
    linear->receive_data(create_message<NumberData>(1, NumberData{1.0}), 2);
    linear->execute();
    linear->receive_data(create_message<NumberData>(2, NumberData{1.0}), 0);
    linear->receive_data(create_message<NumberData>(2, NumberData{1.0}), 1);
    linear->receive_data(create_message<NumberData>(2, NumberData{1.0}), 2);

    Bytes state = linear->collect();
    auto restored = std::make_shared<Linear>("linear", std::vector<double>{1.0, 2.0, 3.0});
    auto it = state.cbegin();
    restored->restore(it);
  
    
    SECTION("verifying deserialization") {
      REQUIRE(*restored == *linear);
    }
  }
}

SCENARIO("Linear operator handles numerical stability", "[linear]") {
  GIVEN("A Linear operator with large coefficients") {
    std::vector<double> coeffs = {1e6, -1e6};  // Large opposing coefficients
    auto linear = std::make_shared<Linear>("linear", coeffs);

    WHEN("Processing values that could cause cancellation") {
      linear->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      linear->receive_data(create_message<NumberData>(1, NumberData{1.0}), 1);
      linear->execute();

      THEN("Results remain numerically stable") {
        const auto& output = linear->get_output_queue(0);
        REQUIRE(!output.empty());
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->data.value == Approx(0.0).margin(1e-10));
      }
    }
  }
}