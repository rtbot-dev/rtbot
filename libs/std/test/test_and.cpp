#include <catch2/catch.hpp>

#include "rtbot/std/And.h"

using namespace rtbot;

SCENARIO("And operator performs logical conjunction of synchronized boolean inputs", "[And]") {
  GIVEN("An And operator") {
    auto op = std::make_unique<And>("and1");

    WHEN("Receiving synchronized true inputs") {
      op->receive_data(create_message<BooleanData>(37, BooleanData{true}), 0);
      op->receive_data(create_message<BooleanData>(37, BooleanData{true}), 1);
      op->execute();

      THEN("Output is true") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<BooleanData>*>(output[0].get());
        REQUIRE(msg->time == 37);
        REQUIRE(msg->data.value == true);
      }
    }

    WHEN("Receiving mixed synchronized inputs") {
      op->receive_data(create_message<BooleanData>(42, BooleanData{true}), 0);
      op->receive_data(create_message<BooleanData>(42, BooleanData{false}), 1);
      op->execute();

      THEN("Output is false") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<BooleanData>*>(output[0].get());
        REQUIRE(msg->time == 42);
        REQUIRE(msg->data.value == false);
      }
    }

    WHEN("Processing streams with different throughput and time gaps") {
      // First stream has regular intervals
      op->receive_data(create_message<BooleanData>(100, BooleanData{true}), 0);
      op->receive_data(create_message<BooleanData>(200, BooleanData{true}), 0);
      op->receive_data(create_message<BooleanData>(300, BooleanData{false}), 0);
      op->receive_data(create_message<BooleanData>(400, BooleanData{true}), 0);
      op->execute();

      // Second stream has irregular intervals and gaps
      op->receive_data(create_message<BooleanData>(100, BooleanData{false}), 1);
      op->receive_data(create_message<BooleanData>(300, BooleanData{true}), 1);
      op->receive_data(create_message<BooleanData>(400, BooleanData{true}), 1);
      op->execute();

      THEN("Only synchronized messages produce output") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 3);

        // t=100: true && false = false
        {
          const auto* msg = dynamic_cast<const Message<BooleanData>*>(output[0].get());
          REQUIRE(msg->time == 100);
          REQUIRE(msg->data.value == false);
        }

        // t=300: false && true = false
        {
          const auto* msg = dynamic_cast<const Message<BooleanData>*>(output[1].get());
          REQUIRE(msg->time == 300);
          REQUIRE(msg->data.value == false);
        }

        // t=400: true && true = true
        {
          const auto* msg = dynamic_cast<const Message<BooleanData>*>(output[2].get());
          REQUIRE(msg->time == 400);
          REQUIRE(msg->data.value == true);
        }
      }

      THEN("Messages at t=200 are properly buffered and discarded") {
        const auto& output = op->get_output_queue(0);
        for (const auto& msg : output) {
          REQUIRE(msg->time != 200);
        }
      }
    }

    WHEN("Processing messages with large time gaps") {
      op->receive_data(create_message<BooleanData>(1000, BooleanData{true}), 0);
      op->receive_data(create_message<BooleanData>(5000, BooleanData{false}), 0);
      op->receive_data(create_message<BooleanData>(1000, BooleanData{true}), 1);
      op->execute();

      THEN("Synchronization works across large time intervals") {
        const auto& output = op->get_output_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<BooleanData>*>(output[0].get());
        REQUIRE(msg->time == 1000);
        REQUIRE(msg->data.value == true);
      }
    }
  }
}