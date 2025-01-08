#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/Demultiplexer.h"

using namespace rtbot;

SCENARIO("Demultiplexer routes messages based on control signals", "[demultiplexer]") {
  GIVEN("A demultiplexer with two output ports") {
    auto demux = std::make_unique<Demultiplexer<NumberData>>("demux", 2);

    WHEN("Multiple control ports are active") {
      // Controls for t=100
      demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 0);
      demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 1);
      demux->receive_data(create_message<NumberData>(100, NumberData{42.0}), 0);
      demux->execute();

      THEN("Message is routed to both ports") {
        const auto& first_output = demux->get_output_queue(0);
        const auto& second_output = demux->get_output_queue(1);

        REQUIRE(first_output.size() == 1);
        REQUIRE(second_output.size() == 1);

        auto* msg1 = dynamic_cast<const Message<NumberData>*>(first_output[0].get());
        auto* msg2 = dynamic_cast<const Message<NumberData>*>(second_output[0].get());

        REQUIRE(msg1->time == 100);
        REQUIRE(msg1->data.value == 42.0);
        REQUIRE(msg2->time == 100);
        REQUIRE(msg2->data.value == 42.0);
      }
    }

    WHEN("No control ports are active") {
      demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 0);
      demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 1);
      demux->receive_data(create_message<NumberData>(100, NumberData{42.0}), 0);
      demux->execute();

      THEN("Message is not routed to any port") {
        const auto& first_output = demux->get_output_queue(0);
        const auto& second_output = demux->get_output_queue(1);

        REQUIRE(first_output.empty());
        REQUIRE(second_output.empty());
      }
    }
  }
}

SCENARIO("Demultiplexer handles sequential routing patterns", "[demultiplexer]") {
  GIVEN("A demultiplexer with three output ports") {
    auto demux = std::make_unique<Demultiplexer<NumberData>>("demux", 3);

    WHEN("Control patterns change over time with increasing timestamps") {
      // t=100: Route to first port only
      demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 0);
      demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 1);
      demux->receive_control(create_message<BooleanData>(100, BooleanData{false}), 2);
      demux->receive_data(create_message<NumberData>(100, NumberData{10.0}), 0);
      demux->execute();

      // t=200: Route to second and third ports
      demux->receive_control(create_message<BooleanData>(200, BooleanData{false}), 0);
      demux->receive_control(create_message<BooleanData>(200, BooleanData{true}), 1);
      demux->receive_control(create_message<BooleanData>(200, BooleanData{true}), 2);
      demux->receive_data(create_message<NumberData>(200, NumberData{20.0}), 0);
      demux->execute();

      // t=300: Route to all ports
      demux->receive_control(create_message<BooleanData>(300, BooleanData{true}), 0);
      demux->receive_control(create_message<BooleanData>(300, BooleanData{true}), 1);
      demux->receive_control(create_message<BooleanData>(300, BooleanData{true}), 2);
      demux->receive_data(create_message<NumberData>(300, NumberData{30.0}), 0);
      demux->execute();

      THEN("Messages are routed correctly with proper timestamps") {
        const auto& output0 = demux->get_output_queue(0);
        const auto& output1 = demux->get_output_queue(1);
        const auto& output2 = demux->get_output_queue(2);

        // Check first output (messages from t=100, t=300)
        REQUIRE(output0.size() == 2);
        auto* msg0_1 = dynamic_cast<const Message<NumberData>*>(output0[0].get());
        auto* msg0_2 = dynamic_cast<const Message<NumberData>*>(output0[1].get());
        REQUIRE(msg0_1->time == 100);
        REQUIRE(msg0_1->data.value == 10.0);
        REQUIRE(msg0_2->time == 300);
        REQUIRE(msg0_2->data.value == 30.0);

        // Check second output (messages from t=200, t=300)
        REQUIRE(output1.size() == 2);
        auto* msg1_1 = dynamic_cast<const Message<NumberData>*>(output1[0].get());
        auto* msg1_2 = dynamic_cast<const Message<NumberData>*>(output1[1].get());
        REQUIRE(msg1_1->time == 200);
        REQUIRE(msg1_1->data.value == 20.0);
        REQUIRE(msg1_2->time == 300);
        REQUIRE(msg1_2->data.value == 30.0);

        // Check third output (messages from t=200, t=300)
        REQUIRE(output2.size() == 2);
        auto* msg2_1 = dynamic_cast<const Message<NumberData>*>(output2[0].get());
        auto* msg2_2 = dynamic_cast<const Message<NumberData>*>(output2[1].get());
        REQUIRE(msg2_1->time == 200);
        REQUIRE(msg2_1->data.value == 20.0);
        REQUIRE(msg2_2->time == 300);
        REQUIRE(msg2_2->data.value == 30.0);
      }
    }
  }
}

SCENARIO("Demultiplexer handles timing and cleanup", "[demultiplexer]") {
  GIVEN("A demultiplexer with two ports") {
    auto demux = std::make_unique<Demultiplexer<NumberData>>("demux", 2);

    WHEN("Data arrives with missing control messages") {
      demux->receive_data(create_message<NumberData>(100, NumberData{10.0}), 0);
      demux->execute();

      THEN("No output is produced") {
        const auto& output0 = demux->get_output_queue(0);
        const auto& output1 = demux->get_output_queue(1);

        REQUIRE(output0.empty());
        REQUIRE(output1.empty());
      }
    }
  }
}

SCENARIO("Demultiplexer handles state serialization", "[demultiplexer]") {
  GIVEN("A demultiplexer with active state") {
    auto demux = std::make_unique<Demultiplexer<NumberData>>("demux", 2);

    // Set up initial state
    demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 0);
    demux->receive_control(create_message<BooleanData>(100, BooleanData{true}), 1);
    demux->receive_data(create_message<NumberData>(100, NumberData{42.0}), 0);

    WHEN("State is serialized and restored") {
      Bytes state = demux->collect();
      auto restored = std::make_unique<Demultiplexer<NumberData>>("demux", 2);
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Behavior matches original") {
        demux->execute();
        restored->execute();

        const auto& orig_output0 = demux->get_output_queue(0);
        const auto& orig_output1 = demux->get_output_queue(1);
        const auto& rest_output0 = restored->get_output_queue(0);
        const auto& rest_output1 = restored->get_output_queue(1);

        REQUIRE(orig_output0.size() == rest_output0.size());
        REQUIRE(orig_output1.size() == rest_output1.size());

        if (!orig_output0.empty()) {
          auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output0[0].get());
          auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output0[0].get());
          REQUIRE(orig_msg->time == rest_msg->time);
          REQUIRE(orig_msg->data.value == rest_msg->data.value);
        }
      }
    }
  }
}