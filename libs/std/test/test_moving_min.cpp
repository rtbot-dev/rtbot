#include <catch2/catch.hpp>

#include "rtbot/std/MovingMin.h"

using namespace rtbot;

SCENARIO("MovingMin operator computes rolling minimums", "[moving_min]") {
  GIVEN("A MovingMin with window size 3") {
    MovingMin op("min", 3);

    WHEN("buffer is not yet full") {
      op.receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op.execute();
      THEN("no output is produced") { REQUIRE(op.get_output_queue(0).empty()); }
    }

    WHEN("buffer becomes full") {
      op.receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op.execute();
      op.receive_data(create_message<NumberData>(2, NumberData{4.0}), 0);
      op.execute();
      op.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
      op.execute();

      THEN("minimum of first window is emitted") {
        const auto& output = op.get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->data.value == Approx(4.0));
        REQUIRE(msg->time == 3);
      }

      AND_WHEN("window slides forward") {
        op.clear_all_output_ports();
        op.receive_data(create_message<NumberData>(4, NumberData{1.0}), 0);
        op.execute();
        const auto& output = op.get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 4);
        REQUIRE(msg->data.value == Approx(1.0));
      }
    }
  }
}
