#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/Demultiplexer.h"

using namespace rtbot;

SCENARIO("Demultiplexer routes messages based on control signals", "[demultiplexer]") {
  GIVEN("A demultiplexer with two output ports") {
    auto demux = std::make_unique<Demultiplexer<NumberData>>("demux", 2);

    WHEN("Control messages arrive before data message") {
      // Set controls for t=100: route to first port
      demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 0);
      demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 1);

      // Send data
      demux->receive_data(create_message<NumberData>(100, NumberData{42.0}), 0);
      demux->execute();

      THEN("Message is routed to first port") {
        const auto& first_output = demux->get_output_queue(0);
        const auto& second_output = demux->get_output_queue(1);

        REQUIRE(first_output.size() == 1);
        REQUIRE(second_output.empty());

        auto* msg = dynamic_cast<const Message<NumberData>*>(first_output[0].get());
        REQUIRE(msg->time == 100);
        REQUIRE(msg->data.value == 42.0);
      }
    }

    WHEN("Multiple messages arrive in sequence") {
      // t=100: route to first port
      demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 0);
      demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 1);
      demux->receive_data(create_message<NumberData>(100, NumberData{42.0}), 0);

      // t=200: route to second port
      demux->receive_control(create_message<BooleanData>(200, BooleanData{false}), 0);
      demux->receive_control(create_message<BooleanData>(200, BooleanData{true}), 1);
      demux->receive_data(create_message<NumberData>(200, NumberData{84.0}), 0);

      demux->execute();

      THEN("Messages are routed to correct ports") {
        const auto& first_output = demux->get_output_queue(0);
        const auto& second_output = demux->get_output_queue(1);

        REQUIRE(first_output.size() == 1);
        REQUIRE(second_output.size() == 1);

        auto* msg1 = dynamic_cast<const Message<NumberData>*>(first_output[0].get());
        REQUIRE(msg1->time == 100);
        REQUIRE(msg1->data.value == 42.0);

        auto* msg2 = dynamic_cast<const Message<NumberData>*>(second_output[0].get());
        REQUIRE(msg2->time == 200);
        REQUIRE(msg2->data.value == 84.0);
      }
    }

    WHEN("Data message arrives before control messages") {
      demux->receive_data(create_message<NumberData>(100, NumberData{42.0}), 0);

      demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 0);
      demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 1);

      demux->execute();

      THEN("Message is still routed correctly") {
        const auto& first_output = demux->get_output_queue(0);
        REQUIRE(first_output.size() == 1);

        auto* msg = dynamic_cast<const Message<NumberData>*>(first_output[0].get());
        REQUIRE(msg->time == 100);
        REQUIRE(msg->data.value == 42.0);
      }
    }

    WHEN("Data arrives without matching control messages") {
      demux->receive_data(create_message<NumberData>(50, NumberData{42.0}), 0);

      demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 0);
      demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 1);

      demux->execute();

      THEN("Old data message is cleaned up") {
        const auto& first_output = demux->get_output_queue(0);
        const auto& second_output = demux->get_output_queue(1);

        REQUIRE(first_output.empty());
        REQUIRE(second_output.empty());
      }
    }

    WHEN("Multiple control ports are active") {
      demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 0);
      demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 1);
      demux->receive_data(create_message<NumberData>(100, NumberData{42.0}), 0);

      THEN("Runtime exception is thrown") {
        REQUIRE_THROWS_AS(demux->execute(), std::runtime_error);
        REQUIRE_THROWS_WITH(demux->execute(), "Multiple control ports active at the same time");
      }
    }
  }
}

SCENARIO("Demultiplexer handles complex message patterns", "[demultiplexer]") {
  GIVEN("A demultiplexer with three output ports") {
    auto demux = std::make_unique<Demultiplexer<NumberData>>("demux", 3);

    WHEN("Messages arrive in an irregular pattern") {
      THEN("First batch is processed correctly") {
        demux->receive_data(create_message<NumberData>(100, NumberData{1.0}), 0);
        demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 0);
        demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 1);
        demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 2);

        demux->execute();

        const auto& out0 = demux->get_output_queue(0);
        REQUIRE(out0.size() == 1);
        auto* msg = dynamic_cast<const Message<NumberData>*>(out0[0].get());
        REQUIRE(msg->time == 100);
        REQUIRE(msg->data.value == 1.0);
      }

      AND_THEN("Second batch routes to different port") {
        demux->receive_data(create_message<NumberData>(150, NumberData{1.5}), 0);
        demux->receive_control(create_message<BooleanData>(150, BooleanData{false}), 0);
        demux->receive_control(create_message<BooleanData>(150, BooleanData{true}), 1);
        demux->receive_control(create_message<BooleanData>(150, BooleanData{false}), 2);

        demux->execute();

        const auto& out1 = demux->get_output_queue(1);
        REQUIRE(out1.size() == 1);
        auto* msg = dynamic_cast<const Message<NumberData>*>(out1[0].get());
        REQUIRE(msg->time == 150);
        REQUIRE(msg->data.value == 1.5);
      }

      AND_THEN("Third batch waits for all controls") {
        demux->receive_data(create_message<NumberData>(200, NumberData{2.0}), 0);
        demux->receive_control(create_message<BooleanData>(200, BooleanData{false}), 0);
        demux->receive_control(create_message<BooleanData>(200, BooleanData{false}), 1);

        demux->execute();

        const auto& out2 = demux->get_output_queue(2);
        REQUIRE(out2.empty());

        demux->receive_control(create_message<BooleanData>(200, BooleanData{true}), 2);
        demux->execute();

        REQUIRE(out2.size() == 1);
        auto* msg = dynamic_cast<const Message<NumberData>*>(out2[0].get());
        REQUIRE(msg->time == 200);
        REQUIRE(msg->data.value == 2.0);
      }
    }
  }
}