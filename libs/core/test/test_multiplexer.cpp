#include <catch2/catch.hpp>

#include "rtbot/Multiplexer.h"

using namespace rtbot;

SCENARIO("Multiplexer routes messages based on control signals", "[multiplexer]") {
  GIVEN("A multiplexer with two ports") {
    auto mult = std::make_unique<Multiplexer<NumberData>>("mult", 2);

    WHEN("Receiving a control message on a data port") {
      REQUIRE_THROWS_AS(mult->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
    }

    WHEN("Receiving a data message on a control port") {
      REQUIRE_THROWS_AS(mult->receive_control(create_message<NumberData>(1, NumberData{42.0}), 0), std::runtime_error);
    }

    WHEN("Receiving valid control and data messages") {
      // Send control messages: select first port
      mult->receive_control(create_message<BooleanData>(1, BooleanData{true}),
                            0  // first control port
      );
      mult->receive_control(create_message<BooleanData>(1, BooleanData{false}),
                            1  // second control port
      );

      // Send data messages
      mult->receive_data(create_message<NumberData>(1, NumberData{42.0}),
                         0  // first data port
      );
      mult->receive_data(create_message<NumberData>(1, NumberData{24.0}),
                         1  // second data port
      );

      THEN("It forwards data from the selected port") {
        mult->execute();

        const auto& output = mult->get_output_queue(0);
        REQUIRE(output.size() == 1);

        auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 1);
        REQUIRE(msg->data.value == 42.0);
      }
    }

    WHEN("Receiving multiple messages in sequence") {
      // First time step: select first port
      mult->receive_control(create_message<BooleanData>(1, BooleanData{true}), 0);
      mult->receive_control(create_message<BooleanData>(1, BooleanData{false}), 1);
      mult->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
      mult->receive_data(create_message<NumberData>(1, NumberData{24.0}), 1);

      // Second time step: select second port
      mult->receive_control(create_message<BooleanData>(2, BooleanData{false}), 0);
      mult->receive_control(create_message<BooleanData>(2, BooleanData{true}), 1);
      mult->receive_data(create_message<NumberData>(2, NumberData{84.0}), 0);
      mult->receive_data(create_message<NumberData>(2, NumberData{48.0}), 1);
      mult->execute();

      THEN("It forwards data from the correct ports in sequence") {
        const auto& output = mult->get_output_queue(0);
        REQUIRE(output.size() == 2);

        auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1 != nullptr);
        REQUIRE(msg1->time == 1);
        REQUIRE(msg1->data.value == 42.0);

        auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2 != nullptr);
        REQUIRE(msg2->time == 2);
        REQUIRE(msg2->data.value == 48.0);
      }
    }

    WHEN("Receiving multiple messages in sequence and exceeds max_size_per_port()") {
      
      for (int i = 0; i < mult->max_size_per_port() + 5; i ++) {
        mult->receive_control(create_message<BooleanData>(i, BooleanData{i % 2 == 0}), 0);
        mult->receive_control(create_message<BooleanData>(i, BooleanData{i % 2 == 1}), 1);
        mult->receive_data(create_message<NumberData>(i, NumberData{i * 2.0}), 0);
        mult->receive_data(create_message<NumberData>(i, NumberData{i * 3.0}), 1);
      }
      mult->execute();
      
      THEN("It forwards data from the correct ports in sequence, it drops 5 messages") {
        const auto& output = mult->get_output_queue(0);
        REQUIRE(output.size() == mult->max_size_per_port());

        auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1 != nullptr);
        REQUIRE(msg1->time == 5);
        REQUIRE(msg1->data.value == 15.0);        
      }
    }

    WHEN("Receiving control signals with no active port") {
      mult->receive_control(create_message<BooleanData>(1, BooleanData{false}), 0);
      mult->receive_control(create_message<BooleanData>(1, BooleanData{false}), 1);
      mult->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);

      THEN("It doesn't produce output") {
        mult->execute();

        const auto& output = mult->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }

    WHEN("Receiving control signals with multiple active ports") {
      mult->receive_control(create_message<BooleanData>(1, BooleanData{true}), 0);
      mult->receive_control(create_message<BooleanData>(1, BooleanData{true}), 1);
      mult->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);

      THEN("It doesn't produce output") {
        mult->execute();

        const auto& output = mult->get_output_queue(0);
        REQUIRE(output.empty());
      }
    }
  }
}

SCENARIO("Multiplexer state serialization", "[multiplexer]") {
  GIVEN("A multiplexer with active state") {
    auto mult = std::make_unique<Multiplexer<NumberData>>("mult", 2);

    // Setup some state
    mult->receive_control(create_message<BooleanData>(1, BooleanData{true}), 0);
    mult->receive_control(create_message<BooleanData>(1, BooleanData{false}), 1);

    WHEN("State is serialized and restored") {
      // Serialize state
      Bytes state = mult->collect();

      // Create new multiplexer
      auto restored = std::make_unique<Multiplexer<NumberData>>("mult", 2);

      // Restore state
      auto it = state.cbegin();
      restored->restore(it);

      THEN("Behavior is preserved") {
        // Send same data to both multiplexers
        auto send_data = [](auto& m) {
          m->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
          m->execute();
        };

        send_data(mult);
        send_data(restored);

        // Compare outputs
        const auto& orig_output = mult->get_output_queue(0);
        const auto& rest_output = restored->get_output_queue(0);

        REQUIRE(orig_output.size() == rest_output.size());
        if (!orig_output.empty()) {
          auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_output.front().get());
          auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_output.front().get());
          REQUIRE(orig_msg->time == rest_msg->time);
          REQUIRE(orig_msg->data.value == rest_msg->data.value);
        }
      }
    }
  }
}

SCENARIO("Multiplexer handles different throughput scenarios", "[multiplexer]") {
  GIVEN("A multiplexer with two ports") {
    auto mult = std::make_unique<Multiplexer<NumberData>>("mult", 2);

    WHEN("There are gaps in the data stream") {
      // Set control states for t=1,2,3
      mult->receive_control(create_message<BooleanData>(1, BooleanData{true}), 0);
      mult->receive_control(create_message<BooleanData>(1, BooleanData{false}), 1);
      mult->receive_control(create_message<BooleanData>(2, BooleanData{true}), 0);
      mult->receive_control(create_message<BooleanData>(2, BooleanData{false}), 1);
      mult->receive_control(create_message<BooleanData>(3, BooleanData{true}), 0);
      mult->receive_control(create_message<BooleanData>(3, BooleanData{false}), 1);

      // Send data with a gap at t=2
      mult->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      mult->receive_data(create_message<NumberData>(1, NumberData{20.0}), 1);
      // Missing data for t=2
      mult->receive_data(create_message<NumberData>(3, NumberData{30.0}), 0);
      mult->receive_data(create_message<NumberData>(3, NumberData{40.0}), 1);

      mult->execute();

      THEN("Only messages with matching control and data are output") {
        const auto& output = mult->get_output_queue(0);
        REQUIRE(output.size() == 2);

        auto* msg1 = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg1->time == 1);
        REQUIRE(msg1->data.value == 10.0);

        auto* msg2 = dynamic_cast<const Message<NumberData>*>(output[1].get());
        REQUIRE(msg2->time == 3);
        REQUIRE(msg2->data.value == 30.0);
      }
    }
  }
}

SCENARIO("Multiplexer handles irregular message patterns", "[multiplexer]") {
  GIVEN("A multiplexer with three ports") {
    const size_t NUM_PORTS = 3;
    auto multiplexer = Multiplexer<NumberData>("test_multiplexer", NUM_PORTS);

    WHEN("Messages arrive with gaps and all controls present") {
      // First set: t=100
      // Data arrives first
      multiplexer.receive_data(create_message(100, NumberData{1.0}), 0);
      multiplexer.receive_data(create_message(100, NumberData{10.0}), 1);
      multiplexer.receive_data(create_message(100, NumberData{100.0}), 2);

      // Then all controls arrive but multiple are active
      multiplexer.receive_control(create_message(100, BooleanData{true}), 0);
      multiplexer.receive_control(create_message(100, BooleanData{true}), 1);
      multiplexer.receive_control(create_message(100, BooleanData{false}), 2);

      // Second set: t=200
      // Data arrives
      multiplexer.receive_data(create_message(200, NumberData{2.0}), 0);
      multiplexer.receive_data(create_message(200, NumberData{20.0}), 1);
      multiplexer.receive_data(create_message(200, NumberData{200.0}), 2);

      // Controls arrive but none are active
      multiplexer.receive_control(create_message(200, BooleanData{false}), 0);
      multiplexer.receive_control(create_message(200, BooleanData{false}), 1);
      multiplexer.receive_control(create_message(200, BooleanData{false}), 2);

      // Third set: t=300
      // All data arrives
      multiplexer.receive_data(create_message(300, NumberData{3.0}), 0);
      multiplexer.receive_data(create_message(300, NumberData{30.0}), 1);
      multiplexer.receive_data(create_message(300, NumberData{300.0}), 2);

      // All controls arrive with exactly one active
      multiplexer.receive_control(create_message(300, BooleanData{false}), 0);
      multiplexer.receive_control(create_message(300, BooleanData{true}), 1);
      multiplexer.receive_control(create_message(300, BooleanData{false}), 2);
      multiplexer.execute();

      THEN("Only messages with complete controls and single selection should be emitted") {
        auto& output_queue = multiplexer.get_output_queue(0);
        REQUIRE(output_queue.size() == 1);

        AND_THEN("Message should be from port 1 at t=300") {
          auto msg = dynamic_cast<const Message<NumberData>*>(output_queue[0].get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == 300);
          REQUIRE(msg->data.value == 30.0);
        }
      }
    }
    WHEN("Messages arrive sparsely but with complete control sets") {
      // First set: t=100 (missing data on port 2)
      multiplexer.receive_data(create_message(100, NumberData{1.0}), 0);
      multiplexer.receive_data(create_message(100, NumberData{10.0}), 1);
      multiplexer.receive_control(create_message(100, BooleanData{false}), 0);
      multiplexer.receive_control(create_message(100, BooleanData{false}), 1);
      multiplexer.receive_control(create_message(100, BooleanData{true}), 2);

      // Second set: t=200 (complete set, one control active)
      multiplexer.receive_data(create_message(200, NumberData{2.0}), 0);
      multiplexer.receive_data(create_message(200, NumberData{20.0}), 1);
      multiplexer.receive_data(create_message(200, NumberData{200.0}), 2);

      multiplexer.receive_control(create_message(200, BooleanData{false}), 0);
      multiplexer.receive_control(create_message(200, BooleanData{true}), 1);
      multiplexer.receive_control(create_message(200, BooleanData{false}), 2);

      // Third set: t=300 (missing data on port 1)
      multiplexer.receive_data(create_message(300, NumberData{3.0}), 0);
      multiplexer.receive_data(create_message(300, NumberData{300.0}), 2);
      multiplexer.receive_control(create_message(300, BooleanData{true}), 0);
      multiplexer.receive_control(create_message(300, BooleanData{false}), 1);
      multiplexer.receive_control(create_message(300, BooleanData{false}), 2);
      multiplexer.execute();

      THEN("Only messages with complete data and single control selection should be emitted") {
        auto& output_queue = multiplexer.get_output_queue(0);
        REQUIRE(output_queue.size() == 2);

        AND_THEN("First message should be from port 1 at t=200") {
          auto msg = dynamic_cast<const Message<NumberData>*>(output_queue[0].get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == 200);
          REQUIRE(msg->data.value == 20.0);
        }

        AND_THEN("Second message should be from port 0 at t=300") {
          auto msg = dynamic_cast<const Message<NumberData>*>(output_queue[1].get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == 300);
          REQUIRE(msg->data.value == 3.0);
        }
      }
    }

    WHEN("Controls arrive before data") {
      // First set: t=100 (controls arrive first)
      multiplexer.receive_control(create_message(100, BooleanData{false}), 0);
      multiplexer.receive_control(create_message(100, BooleanData{true}), 1);
      multiplexer.receive_control(create_message(100, BooleanData{false}), 2);

      multiplexer.receive_data(create_message(100, NumberData{1.0}), 0);
      multiplexer.receive_data(create_message(100, NumberData{10.0}), 1);
      multiplexer.receive_data(create_message(100, NumberData{100.0}), 2);

      // Second set: t=200 (controls and data interleaved)
      multiplexer.receive_control(create_message(200, BooleanData{false}), 0);
      multiplexer.receive_data(create_message(200, NumberData{2.0}), 0);
      multiplexer.receive_control(create_message(200, BooleanData{false}), 1);
      multiplexer.receive_data(create_message(200, NumberData{20.0}), 1);
      multiplexer.receive_control(create_message(200, BooleanData{true}), 2);
      multiplexer.receive_data(create_message(200, NumberData{200.0}), 2);
      multiplexer.execute();

      THEN("Messages should be emitted when both data and controls are complete") {
        auto& output_queue = multiplexer.get_output_queue(0);
        REQUIRE(output_queue.size() == 2);

        AND_THEN("First message should be from port 1 at t=100") {
          auto msg = dynamic_cast<const Message<NumberData>*>(output_queue[0].get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == 100);
          REQUIRE(msg->data.value == 10.0);
        }

        AND_THEN("Second message should be from port 2 at t=200") {
          auto msg = dynamic_cast<const Message<NumberData>*>(output_queue[1].get());
          REQUIRE(msg != nullptr);
          REQUIRE(msg->time == 200);
          REQUIRE(msg->data.value == 200.0);
        }
      }
    }
  }
}
