#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/Pipeline.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"

using namespace rtbot;

SCENARIO("Pipeline handles basic configuration", "[pipeline]") {
  GIVEN("A pipeline with single number input and output") {
    auto pipeline = std::make_unique<Pipeline>("test_pipe", std::vector<std::string>{PortType::NUMBER},
                                               std::vector<std::string>{PortType::NUMBER});

    THEN("Port configuration is correct") {
      REQUIRE(pipeline->num_data_ports() == 1);
      REQUIRE(pipeline->num_output_ports() == 1);
      REQUIRE(pipeline->get_input_port_types()[0] == PortType::NUMBER);
    }

    WHEN("Configuring with invalid port type") {
      THEN("Error is thrown") {
        REQUIRE_THROWS_AS(Pipeline("invalid_pipe", std::vector<std::string>{"invalid_type"},
                                   std::vector<std::string>{PortType::NUMBER}),
                          std::runtime_error);
      }
    }
  }

  GIVEN("A pipeline with multiple port types") {
    auto pipeline =
        std::make_unique<Pipeline>("multi_pipe", std::vector<std::string>{PortType::NUMBER, PortType::BOOLEAN},
                                   std::vector<std::string>{PortType::VECTOR_NUMBER});

    THEN("Port configurations are correct") {
      const auto& input_types = pipeline->get_input_port_types();
      REQUIRE(input_types.size() == 2);
      REQUIRE(input_types[0] == PortType::NUMBER);
      REQUIRE(input_types[1] == PortType::BOOLEAN);
      REQUIRE(pipeline->num_output_ports() == 1);
    }
  }
}

SCENARIO("Pipeline handles internal operator configuration", "[pipeline]") {
  GIVEN("A pipeline with moving average and peak detector") {
    auto pipeline = std::make_unique<Pipeline>("analysis_pipe", std::vector<std::string>{PortType::NUMBER},
                                               std::vector<std::string>{PortType::NUMBER});

    auto ma = std::make_shared<MovingAverage>("ma1", 3);
    auto peak = std::make_shared<PeakDetector>("peak1", 3);

    WHEN("Configuring internal operators") {
      pipeline->register_operator(ma);
      pipeline->register_operator(peak);
      pipeline->set_entry("ma1");
      pipeline->connect("ma1", "peak1");
      pipeline->add_output_mapping("peak1", 0, 0);

      THEN("Processing works correctly") {
        // Feed enough data to fill moving average buffer first
        pipeline->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
        pipeline->execute();
        pipeline->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
        pipeline->execute();
        pipeline->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
        pipeline->execute();

        // Now send data that will create a peak
        pipeline->receive_data(create_message<NumberData>(4, NumberData{9.0}), 0);
        pipeline->execute();
        pipeline->receive_data(create_message<NumberData>(5, NumberData{3.0}), 0);
        pipeline->execute();
        pipeline->receive_data(create_message<NumberData>(6, NumberData{1.0}), 0);
        pipeline->execute();

        const auto& output = pipeline->get_output_queue(0);
        REQUIRE(!output.empty());
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 5);
        REQUIRE(msg->data.value == Approx(5.0));
      }
    }

    WHEN("Trying to connect non-existent operators") {
      pipeline->register_operator(ma);

      THEN("Error is thrown") { REQUIRE_THROWS_AS(pipeline->connect("ma1", "non_existent"), std::runtime_error); }
    }

    WHEN("Setting invalid entry point") {
      THEN("Error is thrown") { REQUIRE_THROWS_AS(pipeline->set_entry("non_existent"), std::runtime_error); }
    }

    WHEN("Adding invalid output mapping") {
      pipeline->register_operator(peak);

      THEN("Error is thrown") { REQUIRE_THROWS_AS(pipeline->add_output_mapping("peak1", 0, 999), std::runtime_error); }
    }
  }
}

SCENARIO("Pipeline handles state reset correctly", "[pipeline]") {
  GIVEN("A pipeline with stateful operators") {
    auto pipeline = std::make_unique<Pipeline>("stateful_pipe", std::vector<std::string>{PortType::NUMBER},
                                               std::vector<std::string>{PortType::NUMBER});

    auto ma = std::make_shared<MovingAverage>("ma1", 2);
    pipeline->register_operator(ma);
    pipeline->set_entry("ma1");
    pipeline->add_output_mapping("ma1", 0, 0);

    WHEN("Processing produces output") {
      // Fill the moving average buffer
      pipeline->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
      pipeline->execute();
      pipeline->receive_data(create_message<NumberData>(2, NumberData{2.0}), 0);
      pipeline->execute();

      THEN("Output is produced") {
        const auto& output = pipeline->get_output_queue(0);
        REQUIRE(!output.empty());
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 2);
        REQUIRE(msg->data.value == Approx(1.5));  // Average of 1.0 and 2.0

        AND_THEN("State is reset for next cycle") {
          pipeline->clear_all_output_ports();
          // Next message should start fresh
          pipeline->receive_data(create_message<NumberData>(3, NumberData{3.0}), 0);
          pipeline->execute();
          REQUIRE(pipeline->get_output_queue(0).empty());
        }
      }
    }
  }
}

SCENARIO("Pipeline handles type checking", "[pipeline]") {
  GIVEN("A pipeline with number input") {
    auto pipeline = std::make_unique<Pipeline>("type_check_pipe", std::vector<std::string>{PortType::NUMBER},
                                               std::vector<std::string>{PortType::NUMBER});

    auto ma = std::make_shared<MovingAverage>("ma1", 2);
    pipeline->register_operator(ma);
    pipeline->set_entry("ma1");
    pipeline->add_output_mapping("ma1", 0, 0);

    WHEN("Receiving wrong message type") {
      THEN("Error is thrown") {
        REQUIRE_THROWS_AS(pipeline->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0),
                          std::runtime_error);
      }
    }
  }
}

SCENARIO("Pipeline handles state serialization correctly", "[pipeline][State]") {
  GIVEN("A pipeline with base operator state") {
    // Create pipeline and verify base operator serialization
    auto pipeline = std::make_unique<Pipeline>("serial_pipe", std::vector<std::string>{PortType::NUMBER},
                                               std::vector<std::string>{PortType::NUMBER});

    WHEN("State is serialized and restored") {
      // Serialize base state
      Bytes state = pipeline->collect();

      // Create new pipeline and restore
      auto restored = std::make_unique<Pipeline>("serial_pipe", std::vector<std::string>{PortType::NUMBER},
                                                 std::vector<std::string>{PortType::NUMBER});

      auto it = state.cbegin();
      restored->restore(it);

      THEN("Base operator state is preserved") {
        REQUIRE(restored->id() == pipeline->id());
        REQUIRE(restored->num_data_ports() == pipeline->num_data_ports());
        REQUIRE(restored->num_output_ports() == pipeline->num_output_ports());
        REQUIRE(*restored==*pipeline);
      }
    }
  }

  GIVEN("A pipeline with registered operators and connections") {
    // Create pipeline and set up operators/connections
    auto pipeline = std::make_unique<Pipeline>("complex_pipe", std::vector<std::string>{PortType::NUMBER},
                                               std::vector<std::string>{PortType::NUMBER});

    auto ma = std::make_shared<MovingAverage>("ma1", 3);
    auto peak = std::make_shared<PeakDetector>("peak1", 3);

    // Register operators and set up configuration
    pipeline->register_operator(ma);
    pipeline->register_operator(peak);
    pipeline->set_entry("ma1");
    pipeline->connect("ma1", "peak1");
    pipeline->add_output_mapping("peak1", 0, 0);

    WHEN("Pipeline state is serialized") {
      Bytes state = pipeline->collect();

      AND_WHEN("State is restored to new pipeline") {
        auto restored = std::make_unique<Pipeline>("complex_pipe", std::vector<std::string>{PortType::NUMBER},
                                                   std::vector<std::string>{PortType::NUMBER});

        auto it = state.cbegin();
        restored->restore(it);

        THEN("Pipeline requires operator re-registration") {
          // Re-register operators
          auto ma_restored = std::make_shared<MovingAverage>("ma1", 3);
          auto peak_restored = std::make_shared<PeakDetector>("peak1", 3);
          restored->register_operator(ma_restored);
          restored->register_operator(peak_restored);
          restored->set_entry("ma1");
          restored->connect("ma1", "peak1");
          restored->add_output_mapping("peak1", 0, 0);

          // Verify both pipelines process data identically
          pipeline->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
          restored->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);

          REQUIRE(*pipeline==*restored);

          pipeline->execute();
          restored->execute();

          const auto& orig_output = pipeline->get_output_queue(0);
          const auto& rest_output = restored->get_output_queue(0);
          REQUIRE(orig_output.size() == rest_output.size());
        }
      }
    }
  }
}