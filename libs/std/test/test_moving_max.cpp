#include <catch2/catch.hpp>

#include "rtbot/std/MovingMax.h"

using namespace rtbot;

SCENARIO("MovingMax operator computes rolling maximums", "[moving_max]") {
  GIVEN("A MovingMax with window size 3") {
    MovingMax op("max", 3);

    WHEN("buffer is not full") {
      op.receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op.execute();
      THEN("no output is produced") { REQUIRE(op.get_output_queue(0).empty()); }
    }

    WHEN("buffer becomes full") {
      op.receive_data(create_message<NumberData>(1, NumberData{5.0}), 0);
      op.execute();
      op.receive_data(create_message<NumberData>(2, NumberData{7.0}), 0);
      op.execute();
      op.receive_data(create_message<NumberData>(3, NumberData{6.0}), 0);
      op.execute();

      THEN("maximum is emitted") {
        const auto& output = op.get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(7.0));
      }

      AND_WHEN("window slides") {
        op.clear_all_output_ports();
        op.receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
        op.execute();
        const auto& output = op.get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 4);
        REQUIRE(msg->data.value == Approx(7.0));
      }
    }
  }
}
