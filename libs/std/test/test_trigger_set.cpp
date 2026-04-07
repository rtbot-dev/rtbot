#include <catch2/catch.hpp>

#include "rtbot/Input.h"
#include "rtbot/TriggerSet.h"
#include "rtbot/std/MovingSum.h"

using namespace rtbot;

SCENARIO("TriggerSet handles basic configuration", "[trigger_set]") {
  GIVEN("A trigger set with single number input and output") {
    auto ts = std::make_unique<TriggerSet>("test_ts", PortType::NUMBER, PortType::NUMBER);

    THEN("Port configuration is correct") {
      REQUIRE(ts->num_data_ports() == 1);
      REQUIRE(ts->num_output_ports() == 1);
      REQUIRE(ts->get_input_port_type() == PortType::NUMBER);
      REQUIRE(ts->get_output_port_type() == PortType::NUMBER);
    }

    WHEN("Configuring with invalid input port type") {
      THEN("Error is thrown") {
        REQUIRE_THROWS_AS(TriggerSet("invalid_ts", "invalid_type", PortType::NUMBER), std::runtime_error);
      }
    }

    WHEN("Configuring with invalid output port type") {
      THEN("Error is thrown") {
        REQUIRE_THROWS_AS(TriggerSet("invalid_ts", PortType::NUMBER, "invalid_type"), std::runtime_error);
      }
    }
  }
}

SCENARIO("TriggerSet handles internal operator configuration", "[trigger_set]") {
  GIVEN("A trigger set with input and moving sum") {
    auto ts = std::make_unique<TriggerSet>("analysis_ts", PortType::NUMBER, PortType::NUMBER);

    auto in = make_number_input("in1");
    auto sum = make_moving_sum("sum1", 3);

    WHEN("Configuring internal operators") {
      ts->register_operator(in);
      ts->register_operator(sum);
      ts->set_entry("in1");
      ts->connect("in1", "sum1");
      ts->set_output("sum1", 0);

      THEN("Trigger fires only after the moving window is full") {
        ts->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
        ts->execute();
        REQUIRE(ts->get_output_queue(0).empty());

        ts->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
        ts->execute();
        REQUIRE(ts->get_output_queue(0).empty());

        ts->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
        ts->execute();

        const auto& output = ts->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(6.0));  // 1 + 2 + 3
      }
    }

    WHEN("Trying to connect non-existent operators") {
      ts->register_operator(in);

      THEN("Error is thrown") { REQUIRE_THROWS_AS(ts->connect("in1", "non_existent"), std::runtime_error); }
    }

    WHEN("Setting invalid entry point") {
      THEN("Error is thrown") { REQUIRE_THROWS_AS(ts->set_entry("non_existent"), std::runtime_error); }
    }

    WHEN("Setting invalid output operator") {
      THEN("Error is thrown") { REQUIRE_THROWS_AS(ts->set_output("non_existent", 0), std::runtime_error); }
    }

    WHEN("Setting output with invalid port") {
      ts->register_operator(sum);

      THEN("Error is thrown") { REQUIRE_THROWS_AS(ts->set_output("sum1", 999), std::runtime_error); }
    }

    WHEN("Executing without entry configured") {
      ts->register_operator(in);

      THEN("Error is thrown") {
        ts->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
        REQUIRE_THROWS_AS(ts->execute(), std::runtime_error);
      }
    }

    WHEN("Executing without output configured") {
      ts->register_operator(in);
      ts->set_entry("in1");

      THEN("Error is thrown") {
        ts->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
        REQUIRE_THROWS_AS(ts->execute(), std::runtime_error);
      }
    }
  }
}

SCENARIO("TriggerSet handles state reset correctly", "[trigger_set]") {
  GIVEN("A trigger set with a stateful moving sum") {
    auto ts = std::make_unique<TriggerSet>("stateful_ts", PortType::NUMBER, PortType::NUMBER);

    auto sum = make_moving_sum("sum1", 2);
    ts->register_operator(sum);
    ts->set_entry("sum1");
    ts->set_output("sum1", 0);

    WHEN("Processing produces output") {
      // Fill the moving sum window (size == 2)
      ts->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      ts->execute();
      ts->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      ts->execute();

      THEN("Output is produced") {
        const auto& output = ts->get_output_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 2);
        REQUIRE(msg->data.value == Approx(3.0));  // 1 + 2

        AND_THEN("State is reset for next cycle") {
          ts->clear_all_output_ports();

          // After reset, the moving window is empty again. A single message
          // is not enough to fill it, so no output should be produced yet.
          ts->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
          ts->execute();
          REQUIRE(ts->get_output_queue(0).empty());

          // A second message refills the window and triggers again.
          ts->receive_data(create_message<NumberData>(4, NumberData{4.0}), 0);
          ts->execute();
          const auto& out2 = ts->get_output_queue(0);
          REQUIRE(out2.size() == 1);
          const auto* msg2 = dynamic_cast<const Message<NumberData>*>(out2[0].get());
          REQUIRE(msg2->data.value == Approx(7.0));  // 3 + 4
        }
      }
    }
  }
}

SCENARIO("TriggerSet handles type checking", "[trigger_set]") {
  GIVEN("A trigger set with number input") {
    auto ts = std::make_unique<TriggerSet>("type_check_ts", PortType::NUMBER, PortType::NUMBER);

    auto sum = make_moving_sum("sum1", 2);
    ts->register_operator(sum);
    ts->set_entry("sum1");
    ts->set_output("sum1", 0);

    WHEN("Receiving wrong message type") {
      THEN("Error is thrown") {
        REQUIRE_THROWS_AS(ts->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0), std::runtime_error);
      }
    }
  }
}

SCENARIO("TriggerSet handles state serialization correctly", "[trigger_set][State]") {
  GIVEN("A trigger set with base operator state") {
    auto ts = std::make_unique<TriggerSet>("serial_ts", PortType::NUMBER, PortType::NUMBER);

    WHEN("State is serialized and restored") {
      auto state = ts->collect();

      auto restored = std::make_unique<TriggerSet>("serial_ts", PortType::NUMBER, PortType::NUMBER);
      restored->restore_data_from_json(state);

      THEN("Base operator state is preserved") {
        REQUIRE(restored->id() == ts->id());
        REQUIRE(restored->num_data_ports() == ts->num_data_ports());
        REQUIRE(restored->num_output_ports() == ts->num_output_ports());
        REQUIRE(*restored == *ts);
      }
    }
  }

  GIVEN("A trigger set with registered operators and connections") {
    auto ts = std::make_unique<TriggerSet>("complex_ts", PortType::NUMBER, PortType::NUMBER);

    auto in = make_number_input("in1");
    auto sum = make_moving_sum("sum1", 3);

    ts->register_operator(in);
    ts->register_operator(sum);
    ts->set_entry("in1");
    ts->connect("in1", "sum1");
    ts->set_output("sum1", 0);

    WHEN("TriggerSet state is serialized") {
      auto state = ts->collect();

      AND_WHEN("State is restored to a new trigger set") {
        auto restored = std::make_unique<TriggerSet>("complex_ts", PortType::NUMBER, PortType::NUMBER);
        restored->restore_data_from_json(state);

        THEN("TriggerSet requires operator re-registration") {
          auto in_restored = make_number_input("in1");
          auto sum_restored = make_moving_sum("sum1", 3);
          restored->register_operator(in_restored);
          restored->register_operator(sum_restored);
          restored->set_entry("in1");
          restored->connect("in1", "sum1");
          restored->set_output("sum1", 0);

          // Verify both trigger sets process data identically
          ts->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
          restored->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);

          REQUIRE(*ts == *restored);

          ts->execute();
          restored->execute();

          const auto& orig_output = ts->get_output_queue(0);
          const auto& rest_output = restored->get_output_queue(0);
          REQUIRE(orig_output.size() == rest_output.size());
        }
      }
    }
  }
}
