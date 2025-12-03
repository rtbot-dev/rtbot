#include <catch2/catch.hpp>

#include "rtbot/std/MovingVariance.h"

using namespace rtbot;

SCENARIO("MovingVariance operator computes rolling variance", "[moving_variance]") {
  GIVEN("A MovingVariance with window size 3") {
    MovingVariance op("var", 3);

    WHEN("buffer is not full") {
      op.receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      op.execute();
      THEN("no output is produced") { REQUIRE(op.get_output_queue(0).empty()); }
    }

    WHEN("buffer becomes full") {
      op.receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      op.execute();
      op.receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      op.execute();
      op.receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
      op.execute();

      THEN("variance is emitted") {
        const auto& output = op.get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(1.0));  // variance of [1,2,3] with ddof=1
      }
    }
  }
}
