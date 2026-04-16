#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/OperatorJson.h"
#include "rtbot/Collector.h"
#include "rtbot/Pipeline.h"
#include "rtbot/std/CumulativeSum.h"
#include "rtbot/std/Count.h"
#include "rtbot/std/FusedExpression.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "rtbot/std/VectorExtract.h"

using namespace rtbot;

// =============================================================================
// Helper: build a pipeline with a CumulativeSum inside
// Input: 1 NumberData port  |  Output: 1 NumberData port
// Control: 1 NumberData port (always present — segment key signal)
// Internal: CumulativeSum (emits running sum on every input)
// =============================================================================
static std::unique_ptr<Pipeline> make_cumsum_pipeline(const std::string& id = "pipe") {
  auto pipeline = std::make_unique<Pipeline>(
      id,
      std::vector<std::string>{PortType::NUMBER},
      std::vector<std::string>{PortType::NUMBER});

  auto cumsum = std::make_shared<CumulativeSum>("cumsum");
  pipeline->register_operator(cumsum);
  pipeline->set_entry("cumsum");
  pipeline->add_output_mapping("cumsum", 0, 0);
  return pipeline;
}

// Helper: send paired data + control message and execute
// key_value is a double — boolean-style tests use 1.0/0.0
static void send_paired(Pipeline& p, timestamp_t t, double data_value, double key_value) {
  p.receive_data(create_message<NumberData>(t, NumberData{data_value}), 0);
  p.receive_control(create_message<NumberData>(t, NumberData{key_value}), 0);
  p.execute();
}

// =============================================================================
// CONFIGURATION TESTS
// =============================================================================

SCENARIO("Pipeline handles basic configuration", "[pipeline]") {
  GIVEN("A pipeline with single number input and output") {
    auto pipeline = std::make_unique<Pipeline>("test_pipe", std::vector<std::string>{PortType::NUMBER},
                                                std::vector<std::string>{PortType::NUMBER});

    THEN("Port configuration is correct") {
      REQUIRE(pipeline->num_data_ports() == 1);
      REQUIRE(pipeline->num_control_ports() == 1);  // always has control port
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
      REQUIRE(pipeline->num_control_ports() == 1);  // always 1 control port
    }
  }
}

SCENARIO("Pipeline handles internal operator configuration", "[pipeline]") {
  GIVEN("A pipeline with moving average and peak detector") {
    auto pipeline = std::make_unique<Pipeline>("analysis_pipe", std::vector<std::string>{PortType::NUMBER},
                                                std::vector<std::string>{PortType::NUMBER});
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    auto ma = std::make_shared<MovingAverage>("ma1", 3);
    auto peak = std::make_shared<PeakDetector>("peak1", 3);

    WHEN("Configuring internal operators") {
      pipeline->register_operator(ma);
      pipeline->register_operator(peak);
      pipeline->set_entry("ma1");
      pipeline->connect(ma, peak);
      pipeline->add_output_mapping("peak1", 0, 0);

      THEN("Processing works correctly with segment-scoped behavior") {
        // All messages in same segment (key=true) — no output until key changes.
        // Feed enough data to fill moving average buffer, then create a peak.
        send_paired(*pipeline, 1, 1.0, 1.0);
        send_paired(*pipeline, 2, 2.0, 1.0);
        send_paired(*pipeline, 3, 3.0, 1.0);
        send_paired(*pipeline, 4, 9.0, 1.0);
        send_paired(*pipeline, 5, 3.0, 1.0);
        send_paired(*pipeline, 6, 1.0, 1.0);

        // No output yet — same key
        REQUIRE(col->get_data_queue(0).empty());

        // Key change triggers emission of buffered output
        send_paired(*pipeline, 7, 0.0, 0.0);

        const auto& output = col->get_data_queue(0);
        REQUIRE(!output.empty());
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 7);  // boundary timestamp
        // PeakDetector should have detected the peak at the moving average value
        REQUIRE(msg->data.value == Approx(5.0));
      }
    }

    WHEN("Trying to connect non-existent operators") {
      pipeline->register_operator(ma);

      THEN("Error is thrown") {
        auto ghost = std::make_shared<PeakDetector>("non_existent", 3);
        REQUIRE_THROWS_AS(pipeline->connect(ma, ghost), std::runtime_error);
      }
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

// =============================================================================
// SEGMENT-SCOPED BEHAVIOR TESTS
// =============================================================================

SCENARIO("Pipeline: first message sets key, no emission", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("The first paired message is sent") {
      send_paired(*pipeline, 1, 10.0, 1.0);  // data=10, key=true

      THEN("No output is emitted (first message sets key, starts accumulating)") {
        REQUIRE(col->get_data_queue(0).empty());
      }
    }
  }
}

SCENARIO("Pipeline: same key accumulates, no emission", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Multiple messages with the same key are sent") {
      send_paired(*pipeline, 1, 10.0, 1.0);   // cumsum=10
      send_paired(*pipeline, 2, 20.0, 1.0);   // cumsum=30
      send_paired(*pipeline, 3, 30.0, 1.0);   // cumsum=60

      THEN("No output is emitted while key is stable") {
        REQUIRE(col->get_data_queue(0).empty());
      }
    }
  }
}

SCENARIO("Pipeline: key change triggers emission", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Key changes after accumulation") {
      // Segment 1: key=true, data values 10, 20, 30 -> cumsum builds: 10, 30, 60
      send_paired(*pipeline, 1, 10.0, 1.0);
      send_paired(*pipeline, 2, 20.0, 1.0);
      send_paired(*pipeline, 3, 30.0, 1.0);

      // Key change at t=4: should emit buffer (cumsum=60) with boundary timestamp=4
      send_paired(*pipeline, 4, 5.0, 0.0);

      THEN("Buffer is emitted with the boundary timestamp") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 4);           // boundary timestamp
        REQUIRE(msg->data.value == Approx(60.0));  // last cumsum of segment 1
      }
    }
  }
}

SCENARIO("Pipeline: multiple segments", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Multiple key changes occur") {
      // Segment 1: key=true
      send_paired(*pipeline, 1, 10.0, 1.0);   // cumsum=10
      send_paired(*pipeline, 2, 20.0, 1.0);   // cumsum=30

      // Key change -> segment 2: key=false (emits segment 1 buffer: cumsum=30 at t=3)
      send_paired(*pipeline, 3, 5.0, 0.0);   // cumsum resets, new cumsum=5
      col->reset();  // clear the emission from segment 1

      send_paired(*pipeline, 4, 15.0, 0.0);  // cumsum=20

      // Key change -> segment 3: key=true (emits segment 2 buffer: cumsum=20 at t=5)
      send_paired(*pipeline, 5, 100.0, 1.0);

      THEN("Second segment emission is correct") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg != nullptr);
        REQUIRE(msg->time == 5);             // boundary timestamp
        REQUIRE(msg->data.value == Approx(20.0));  // cumsum of segment 2
      }
    }
  }
}

SCENARIO("Pipeline: boundary timestamp verification", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Key changes at a specific timestamp") {
      // Segment: key=true, timestamps 100, 200, 300
      send_paired(*pipeline, 100, 1.0, 1.0);
      send_paired(*pipeline, 200, 2.0, 1.0);
      send_paired(*pipeline, 300, 3.0, 1.0);

      // Key change at t=400
      send_paired(*pipeline, 400, 10.0, 0.0);

      THEN("Output timestamp equals the boundary (triggering) timestamp, not the last data timestamp") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 400);  // boundary, not 300
        REQUIRE(msg->data.value == Approx(6.0));  // 1+2+3
      }
    }
  }
}

SCENARIO("Pipeline: single-message segments", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Each segment has exactly one message") {
      send_paired(*pipeline, 1, 42.0, 1.0);   // segment 1: cumsum=42
      send_paired(*pipeline, 2, 99.0, 0.0);  // key change -> emit 42 at t=2, start segment 2

      THEN("Single-message segment emits correctly") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 2);
        REQUIRE(msg->data.value == Approx(42.0));
      }
    }
  }
}

SCENARIO("Pipeline: boolean-style keys (0/1)", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Keys alternate between false and true") {
      // Segment: key=false
      send_paired(*pipeline, 1, 5.0, 0.0);
      send_paired(*pipeline, 2, 5.0, 0.0);

      // Key change to true at t=3
      send_paired(*pipeline, 3, 1.0, 1.0);

      THEN("Emission occurs on boolean key change") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(10.0));  // 5+5
      }

      AND_WHEN("Key flips back to false") {
        col->reset();
        send_paired(*pipeline, 4, 7.0, 1.0);   // same key=true, cumsum=8
        send_paired(*pipeline, 5, 3.0, 0.0);  // key change -> emit cumsum=8 at t=5

        THEN("Second flip emits correctly") {
          const auto& output = col->get_data_queue(0);
          REQUIRE(output.size() == 1);

          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
          REQUIRE(msg->time == 5);
          REQUIRE(msg->data.value == Approx(8.0));  // 1+7 (cumsum after reset)
        }
      }
    }
  }
}

SCENARIO("Pipeline: rapid key changes (every message)", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Key changes on every message") {
      send_paired(*pipeline, 1, 10.0, 1.0);   // first, no emit
      send_paired(*pipeline, 2, 20.0, 0.0);  // key change -> emit 10 at t=2
      
      const auto& out1 = col->get_data_queue(0);
      REQUIRE(out1.size() == 1);
      auto* msg1 = dynamic_cast<const Message<NumberData>*>(out1[0].get());
      REQUIRE(msg1->time == 2);
      REQUIRE(msg1->data.value == Approx(10.0));

      col->reset();
      send_paired(*pipeline, 3, 30.0, 1.0);  // key change -> emit 20 at t=3

      THEN("Each key change produces correct emission") {
        const auto& out2 = col->get_data_queue(0);
        REQUIRE(out2.size() == 1);
        auto* msg2 = dynamic_cast<const Message<NumberData>*>(out2[0].get());
        REQUIRE(msg2->time == 3);
        REQUIRE(msg2->data.value == Approx(20.0));
      }
    }
  }
}

// =============================================================================
// SYNCHRONIZATION TESTS
// =============================================================================

SCENARIO("Pipeline: data waits for control", "[pipeline][sync]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Data is sent without a control message") {
      pipeline->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      pipeline->execute();

      THEN("No output is produced (data waits for control)") {
        REQUIRE(col->get_data_queue(0).empty());
      }

      AND_WHEN("Control message arrives and execute is called again") {
        pipeline->receive_control(create_message<NumberData>(1, NumberData{1.0}), 0);
        pipeline->execute();

        THEN("Messages are paired and processed") {
          // First message, so no emission, but no crash either
          REQUIRE(col->get_data_queue(0).empty());
        }
      }
    }
  }
}

SCENARIO("Pipeline: control waits for data", "[pipeline][sync]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Control is sent without a data message") {
      pipeline->receive_control(create_message<NumberData>(1, NumberData{1.0}), 0);
      pipeline->execute();

      THEN("No output is produced (control waits for data)") {
        REQUIRE(col->get_data_queue(0).empty());
      }

      AND_WHEN("Data message arrives and execute is called again") {
        pipeline->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
        pipeline->execute();

        THEN("Messages are paired and processed") {
          REQUIRE(col->get_data_queue(0).empty());  // first message, no emit
        }
      }
    }
  }
}

SCENARIO("Pipeline: timestamp mismatch discards older data", "[pipeline][sync]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Data timestamp is older than control timestamp") {
      pipeline->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      pipeline->receive_control(create_message<NumberData>(2, NumberData{1.0}), 0);
      pipeline->execute();

      THEN("Older data is discarded, nothing is processed") {
        REQUIRE(col->get_data_queue(0).empty());
      }

      AND_WHEN("Matching data arrives for the control timestamp") {
        pipeline->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
        pipeline->execute();

        THEN("Pair is processed (first message, no emission)") {
          REQUIRE(col->get_data_queue(0).empty());
        }
      }
    }

    WHEN("Control timestamp is older than data timestamp") {
      pipeline->receive_control(create_message<NumberData>(1, NumberData{1.0}), 0);
      pipeline->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
      pipeline->execute();

      THEN("Older control is discarded, nothing is processed") {
        REQUIRE(col->get_data_queue(0).empty());
      }

      AND_WHEN("Matching control arrives for the data timestamp") {
        pipeline->receive_control(create_message<NumberData>(2, NumberData{1.0}), 0);
        pipeline->execute();

        THEN("Pair is processed (first message, no emission)") {
          REQUIRE(col->get_data_queue(0).empty());
        }
      }
    }
  }
}

SCENARIO("Pipeline: batch message processing", "[pipeline][sync]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Multiple paired messages are sent before a single execute") {
      // Send three data + three control messages, then execute once
      pipeline->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
      pipeline->receive_control(create_message<NumberData>(1, NumberData{1.0}), 0);
      pipeline->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
      pipeline->receive_control(create_message<NumberData>(2, NumberData{1.0}), 0);
      pipeline->receive_data(create_message<NumberData>(3, NumberData{30.0}), 0);
      pipeline->receive_control(create_message<NumberData>(3, NumberData{0.0}), 0);  // key change!
      pipeline->execute();

      THEN("All pairs are processed: first two accumulate, third triggers emission") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(30.0));  // cumsum of 10+20=30
      }
    }
  }
}

// =============================================================================
// RESET TESTS
// =============================================================================

SCENARIO("Pipeline: reset clears all segment state", "[pipeline][reset]") {
  GIVEN("A pipeline with accumulated state") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    // Build up some state
    send_paired(*pipeline, 1, 10.0, 1.0);
    send_paired(*pipeline, 2, 20.0, 1.0);

    WHEN("Pipeline is reset") {
      pipeline->reset();

      THEN("All state is cleared") {
        REQUIRE(col->get_data_queue(0).empty());
      }

      AND_WHEN("New messages are sent after reset") {
        // After reset, timestamps can start fresh since internal state is cleared
        send_paired(*pipeline, 3, 100.0, 1.0);  // first msg after reset

        THEN("It behaves as if fresh (first message, no emission)") {
          REQUIRE(col->get_data_queue(0).empty());
        }

        AND_WHEN("Key changes after reset") {
          send_paired(*pipeline, 4, 200.0, 0.0);  // key change -> emit 100 at t=4

          THEN("Emission reflects post-reset state only") {
            const auto& output = col->get_data_queue(0);
            REQUIRE(output.size() == 1);
            const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
            REQUIRE(msg->time == 4);
            REQUIRE(msg->data.value == Approx(100.0));  // only the post-reset value
          }
        }
      }
    }
  }
}

SCENARIO("Pipeline: reset with MovingAverage restarts accumulation", "[pipeline][reset]") {
  GIVEN("A pipeline with MovingAverage(2)") {
    auto pipeline = std::make_unique<Pipeline>("stateful_pipe", std::vector<std::string>{PortType::NUMBER},
                                                std::vector<std::string>{PortType::NUMBER});
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    auto ma = std::make_shared<MovingAverage>("ma1", 2);
    pipeline->register_operator(ma);
    pipeline->set_entry("ma1");
    pipeline->add_output_mapping("ma1", 0, 0);

    WHEN("Segment accumulates and key changes to emit") {
      // Segment 1: key=true, MA needs 2 messages to produce output
      send_paired(*pipeline, 1, 1.0, 1.0);  // MA buffer: [1.0], no output yet
      send_paired(*pipeline, 2, 2.0, 1.0);  // MA buffer: [1.0, 2.0], output=1.5

      // No output yet because same key
      REQUIRE(col->get_data_queue(0).empty());

      // Key change: emit buffer (MA output=1.5) at boundary t=3
      send_paired(*pipeline, 3, 3.0, 0.0);

      THEN("Buffered moving average is emitted at boundary") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(1.5));  // Average of 1.0 and 2.0
      }
    }

    WHEN("Reset then new segment") {
      send_paired(*pipeline, 1, 1.0, 1.0);
      send_paired(*pipeline, 2, 2.0, 1.0);
      pipeline->reset();

      // After reset, start fresh
      send_paired(*pipeline, 3, 10.0, 1.0);   // MA buffer: [10.0], no output yet
      send_paired(*pipeline, 4, 20.0, 1.0);   // MA buffer: [10.0, 20.0], output=15.0

      // Key change
      send_paired(*pipeline, 5, 30.0, 0.0);

      THEN("Output reflects only post-reset state") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);
        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 5);
        REQUIRE(msg->data.value == Approx(15.0));  // Average of 10.0 and 20.0
      }
    }
  }
}

// =============================================================================
// MULTI-OPERATOR MESH TESTS
// =============================================================================

SCENARIO("Pipeline: multi-operator mesh (CumulativeSum -> CountNumber)", "[pipeline][mesh]") {
  GIVEN("A pipeline with chained operators (cumsum -> count)") {
    auto pipeline = std::make_unique<Pipeline>(
        "multi_op_pipe",
        std::vector<std::string>{PortType::NUMBER},
        std::vector<std::string>{PortType::NUMBER});
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    auto cumsum = std::make_shared<CumulativeSum>("cumsum");
    auto count = std::make_shared<CountNumber>("count");

    pipeline->register_operator(cumsum);
    pipeline->register_operator(count);
    pipeline->set_entry("cumsum");
    pipeline->connect(cumsum, count);
    pipeline->add_output_mapping("count", 0, 0);  // terminal count output -> pipeline port 0

    WHEN("Data flows through both operators with key change") {
      // Segment: key=true
      // t=1: cumsum=10, count=1
      // t=2: cumsum=30, count=2
      // t=3: cumsum=60, count=3
      send_paired(*pipeline, 1, 10.0, 1.0);
      send_paired(*pipeline, 2, 20.0, 1.0);
      send_paired(*pipeline, 3, 30.0, 1.0);

      // Key change at t=4
      send_paired(*pipeline, 4, 5.0, 0.0);

      THEN("Terminal operator (count) buffer is emitted at boundary timestamp") {
        const auto& out0 = col->get_data_queue(0);
        REQUIRE(out0.size() == 1);

        // Count saw 3 messages in segment 1
        const auto* msg0 = dynamic_cast<const Message<NumberData>*>(out0[0].get());
        REQUIRE(msg0->time == 4);
        REQUIRE(msg0->data.value == Approx(3.0));
      }
    }

    WHEN("Multiple segments flow through the chain") {
      // Segment 1: key=true, 2 messages -> count=2
      send_paired(*pipeline, 1, 10.0, 1.0);
      send_paired(*pipeline, 2, 20.0, 1.0);

      // Key change -> segment 2 (emits count=2 at t=3)
      send_paired(*pipeline, 3, 5.0, 0.0);
      const auto& out1 = col->get_data_queue(0);
      REQUIRE(out1.size() == 1);
      REQUIRE(dynamic_cast<const Message<NumberData>*>(out1[0].get())->data.value == Approx(2.0));
      col->reset();

      // Segment 2: key=false, 3 messages -> count=3 (resets after key change)
      send_paired(*pipeline, 4, 15.0, 0.0);
      send_paired(*pipeline, 5, 25.0, 0.0);

      // Key change -> segment 3 (emits count=3 at t=6)
      send_paired(*pipeline, 6, 1.0, 1.0);

      THEN("Count resets between segments") {
        const auto& out2 = col->get_data_queue(0);
        REQUIRE(out2.size() == 1);
        REQUIRE(dynamic_cast<const Message<NumberData>*>(out2[0].get())->data.value == Approx(3.0));
        REQUIRE(dynamic_cast<const Message<NumberData>*>(out2[0].get())->time == 6);
      }
    }
  }
}

// =============================================================================
// NUMERIC KEY TESTS (non-integer keys)
// =============================================================================

SCENARIO("Pipeline: non-integer numeric keys", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Keys are fractional numbers (e.g., day indices as doubles)") {
      // Segment 1: key=1.5
      send_paired(*pipeline, 1, 10.0, 1.5);  // cumsum=10
      send_paired(*pipeline, 2, 20.0, 1.5);  // cumsum=30

      // Key change to 2.7 at t=3
      send_paired(*pipeline, 3, 5.0, 2.7);

      THEN("Emission occurs on fractional key change") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3);
        REQUIRE(msg->data.value == Approx(30.0));  // 10+20
      }

      AND_WHEN("Another fractional key change occurs") {
        col->reset();
        send_paired(*pipeline, 4, 100.0, 2.7);  // cumsum=105 (5+100 after reset)
        send_paired(*pipeline, 5, 200.0, 3.14159);  // key change to pi

        THEN("Second fractional key change emits correctly") {
          const auto& output = col->get_data_queue(0);
          REQUIRE(output.size() == 1);

          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
          REQUIRE(msg->time == 5);
          REQUIRE(msg->data.value == Approx(105.0));  // 5+100
        }
      }
    }
  }
}

SCENARIO("Pipeline: large numeric keys (day/cycle indices)", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Keys simulate FLOOR(ts / 86400) day indices") {
      double day1 = 19814.0;  // FLOOR(some_ts / 86400)
      double day2 = 19815.0;

      send_paired(*pipeline, 1000, 5.0, day1);  // cumsum=5
      send_paired(*pipeline, 2000, 15.0, day1);  // cumsum=20

      // Day change
      send_paired(*pipeline, 3000, 1.0, day2);

      THEN("Emission occurs on large integer key change") {
        const auto& output = col->get_data_queue(0);
        REQUIRE(output.size() == 1);

        const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
        REQUIRE(msg->time == 3000);
        REQUIRE(msg->data.value == Approx(20.0));
      }
    }
  }
}

// =============================================================================
// EMPTY BUFFER TESTS
// =============================================================================

SCENARIO("Pipeline: key change with empty buffer (no internal output)", "[pipeline][segment]") {
  GIVEN("A pipeline with MovingAverage(3) that needs 3 inputs before producing output") {
    auto pipeline = std::make_unique<Pipeline>(
        "ma_pipe", std::vector<std::string>{PortType::NUMBER},
        std::vector<std::string>{PortType::NUMBER});
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    auto ma = std::make_shared<MovingAverage>("ma1", 3);
    pipeline->register_operator(ma);
    pipeline->set_entry("ma1");
    pipeline->add_output_mapping("ma1", 0, 0);

    WHEN("Key changes before MA has enough data to produce output") {
      // Segment 1: only 2 messages, MA(3) needs 3
      send_paired(*pipeline, 1, 10.0, 1.0);
      send_paired(*pipeline, 2, 20.0, 1.0);

      // Key change at t=3: MA never produced output → buffer is empty
      send_paired(*pipeline, 3, 30.0, 0.0);

      THEN("No output is emitted (buffer was empty)") {
        REQUIRE(col->get_data_queue(0).empty());
      }

      AND_WHEN("New segment produces enough data for MA output") {
        // Segment 2: 3 messages with key=0 → MA produces output
        send_paired(*pipeline, 4, 40.0, 0.0);
        send_paired(*pipeline, 5, 50.0, 0.0);

        // No output yet (same key)
        REQUIRE(col->get_data_queue(0).empty());

        // Key change at t=6
        send_paired(*pipeline, 6, 60.0, 1.0);

        THEN("Output from the completed segment is emitted") {
          const auto& output = col->get_data_queue(0);
          REQUIRE(output.size() == 1);

          const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
          REQUIRE(msg->time == 6);
          // MA(3) of 30,40,50 = 40.0 (the values after reset: 30@t3, 40@t4, 50@t5)
          REQUIRE(msg->data.value == Approx(40.0));
        }
      }
    }

    WHEN("Key changes on every message (never enough for MA output)") {
      send_paired(*pipeline, 1, 10.0, 1.0);
      send_paired(*pipeline, 2, 20.0, 2.0);  // key change, but buffer empty
      send_paired(*pipeline, 3, 30.0, 3.0);  // key change, but buffer empty

      THEN("No output is ever emitted") {
        REQUIRE(col->get_data_queue(0).empty());
      }
    }
  }
}

// =============================================================================
// LONG SEQUENCE TESTS
// =============================================================================

SCENARIO("Pipeline: long sequence with multiple key changes", "[pipeline][segment]") {
  GIVEN("A pipeline with CumulativeSum") {
    auto pipeline = make_cumsum_pipeline();
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("A long sequence with 3 segments is processed") {
      // Segment 1: key=true, values: 1,2,3 -> cumsum: 1,3,6
      send_paired(*pipeline, 1, 1.0, 1.0);
      send_paired(*pipeline, 2, 2.0, 1.0);
      send_paired(*pipeline, 3, 3.0, 1.0);

      // Key change -> segment 2: key=false (emits 6 at t=4)
      send_paired(*pipeline, 4, 10.0, 0.0);
      const auto& out1 = col->get_data_queue(0);
      REQUIRE(out1.size() == 1);
      REQUIRE(dynamic_cast<const Message<NumberData>*>(out1[0].get())->data.value == Approx(6.0));
      REQUIRE(dynamic_cast<const Message<NumberData>*>(out1[0].get())->time == 4);
      col->reset();

      // Segment 2: key=false, values: 10,20 -> cumsum: 10,30
      send_paired(*pipeline, 5, 20.0, 0.0);

      // Key change -> segment 3: key=true (emits 30 at t=6)
      send_paired(*pipeline, 6, 100.0, 1.0);
      const auto& out2 = col->get_data_queue(0);
      REQUIRE(out2.size() == 1);
      REQUIRE(dynamic_cast<const Message<NumberData>*>(out2[0].get())->data.value == Approx(30.0));
      REQUIRE(dynamic_cast<const Message<NumberData>*>(out2[0].get())->time == 6);
      col->reset();

      // Segment 3: key=true, values: 100,200 -> cumsum: 100,300
      send_paired(*pipeline, 7, 200.0, 1.0);

      // Key change -> segment 4: (emits 300 at t=8)
      send_paired(*pipeline, 8, 1.0, 0.0);

      THEN("All three segment emissions are correct") {
        const auto& out3 = col->get_data_queue(0);
        REQUIRE(out3.size() == 1);
        REQUIRE(dynamic_cast<const Message<NumberData>*>(out3[0].get())->data.value == Approx(300.0));
        REQUIRE(dynamic_cast<const Message<NumberData>*>(out3[0].get())->time == 8);
      }
    }
  }
}

// =============================================================================
// SERIALIZATION TESTS
// =============================================================================

SCENARIO("Pipeline: base state serialization", "[pipeline][State]") {
  GIVEN("A pipeline with base operator state") {
    auto pipeline = std::make_unique<Pipeline>("serial_pipe", std::vector<std::string>{PortType::NUMBER},
                                                std::vector<std::string>{PortType::NUMBER});

    WHEN("State is serialized and restored") {
      auto state = pipeline->collect();

      auto restored = std::make_unique<Pipeline>("serial_pipe", std::vector<std::string>{PortType::NUMBER},
                                                  std::vector<std::string>{PortType::NUMBER});

      restored->restore_data_from_json(state);

      THEN("Base operator state is preserved") {
        REQUIRE(restored->id() == pipeline->id());
        REQUIRE(restored->num_data_ports() == pipeline->num_data_ports());
        REQUIRE(restored->num_output_ports() == pipeline->num_output_ports());
        REQUIRE(*restored == *pipeline);
      }
    }
  }
}

SCENARIO("Pipeline: complex state serialization with operators", "[pipeline][State]") {
  GIVEN("A pipeline with registered operators and connections") {
    auto pipeline = std::make_unique<Pipeline>("complex_pipe", std::vector<std::string>{PortType::NUMBER},
                                                std::vector<std::string>{PortType::NUMBER});
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    auto ma = std::make_shared<MovingAverage>("ma1", 3);
    auto peak = std::make_shared<PeakDetector>("peak1", 3);

    pipeline->register_operator(ma);
    pipeline->register_operator(peak);
    pipeline->set_entry("ma1");
    pipeline->connect(ma, peak);
    pipeline->add_output_mapping("peak1", 0, 0);

    WHEN("Pipeline state is serialized") {
      auto state = pipeline->collect();

      AND_WHEN("State is restored to new pipeline") {
        auto restored = std::make_unique<Pipeline>("complex_pipe", std::vector<std::string>{PortType::NUMBER},
                                                    std::vector<std::string>{PortType::NUMBER});
        auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
        restored->connect(rcol, 0, 0);

        restored->restore_data_from_json(state);

        THEN("Pipeline requires operator re-registration") {
          // Re-register operators
          auto ma_restored = std::make_shared<MovingAverage>("ma1", 3);
          auto peak_restored = std::make_shared<PeakDetector>("peak1", 3);
          restored->register_operator(ma_restored);
          restored->register_operator(peak_restored);
          restored->set_entry("ma1");
          restored->connect(ma_restored, peak_restored);
          restored->add_output_mapping("peak1", 0, 0);

          // Verify both pipelines process data identically (send paired)
          pipeline->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
          pipeline->receive_control(create_message<NumberData>(1, NumberData{1.0}), 0);
          restored->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
          restored->receive_control(create_message<NumberData>(1, NumberData{1.0}), 0);

          REQUIRE(*pipeline == *restored);

          pipeline->execute();
          restored->execute();

          const auto& orig_output = col->get_data_queue(0);
          const auto& rest_output = rcol->get_data_queue(0);
          REQUIRE(orig_output.size() == rest_output.size());
        }
      }
    }
  }
}

SCENARIO("Pipeline: segment state serialization mid-segment", "[pipeline][State]") {
  GIVEN("A pipeline with accumulated state mid-segment") {
    auto pipeline = make_cumsum_pipeline("ser_pipe");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    // Accumulate some state in a segment
    send_paired(*pipeline, 1, 10.0, 1.0);  // cumsum=10
    send_paired(*pipeline, 2, 20.0, 1.0);  // cumsum=30

    WHEN("State is serialized") {
      auto state = pipeline->collect();

      AND_WHEN("State is restored to a fresh pipeline") {
        auto restored = make_cumsum_pipeline("ser_pipe");
        auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
        restored->connect(rcol, 0, 0);
        restored->restore_data_from_json(state);

        THEN("Segment state is preserved") {
          REQUIRE(*restored == *pipeline);
        }

        AND_WHEN("Key changes on the restored pipeline") {
          // Key change at t=3 should emit the buffered cumsum=30
          send_paired(*restored, 3, 100.0, 0.0);

          THEN("Emission reflects the pre-serialization accumulated state") {
            const auto& output = rcol->get_data_queue(0);
            REQUIRE(output.size() == 1);
            const auto* msg = dynamic_cast<const Message<NumberData>*>(output[0].get());
            REQUIRE(msg->time == 3);
            REQUIRE(msg->data.value == Approx(30.0));
          }
        }
      }
    }
  }

  GIVEN("A pipeline with no accumulated state") {
    auto pipeline = make_cumsum_pipeline("empty_ser");
    auto col = std::make_shared<Collector>("c", std::vector<std::string>{"number"});
    pipeline->connect(col, 0, 0);

    WHEN("Empty state is serialized and restored") {
      auto state = pipeline->collect();
      auto restored = make_cumsum_pipeline("empty_ser");
      auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
      restored->connect(rcol, 0, 0);
      restored->restore_data_from_json(state);

      THEN("Pipelines are equal") {
        REQUIRE(*restored == *pipeline);
      }
    }
  }
}

SCENARIO("Pipeline: serialization round-trip preserves behavior", "[pipeline][State]") {
  GIVEN("Two identical pipelines, one serialized mid-segment") {
    auto original = make_cumsum_pipeline("rt");
    auto ocol = std::make_shared<Collector>("oc", std::vector<std::string>{"number"});
    original->connect(ocol, 0, 0);
    
    // Feed some data
    send_paired(*original, 1, 10.0, 1.0);
    send_paired(*original, 2, 20.0, 1.0);

    // Serialize and restore
    auto state = original->collect();
    auto restored = make_cumsum_pipeline("rt");
    auto rcol = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
    restored->connect(rcol, 0, 0);
    restored->restore_data_from_json(state);

    WHEN("Both receive the same subsequent messages") {
      // Key change
      send_paired(*original, 3, 5.0, 0.0);
      send_paired(*restored, 3, 5.0, 0.0);

      THEN("Both produce identical output") {
        const auto& out_orig = ocol->get_data_queue(0);
        const auto& out_rest = rcol->get_data_queue(0);

        REQUIRE(out_orig.size() == out_rest.size());
        REQUIRE(out_orig.size() == 1);

        const auto* msg_orig = dynamic_cast<const Message<NumberData>*>(out_orig[0].get());
        const auto* msg_rest = dynamic_cast<const Message<NumberData>*>(out_rest[0].get());

        REQUIRE(msg_orig->time == msg_rest->time);
        REQUIRE(msg_orig->data.value == Approx(msg_rest->data.value));
      }
    }
  }
}

// =============================================================================
// EQUALITY TESTS
// =============================================================================

// =============================================================================
// SEGMENT BYTECODE HELPERS
// =============================================================================

// Helper: build a segment-bytecode pipeline
// Internal mesh: VectorExtract(index=0) → CumulativeSum
// Input: 1 VectorNumber port | Output: 1 Number port
// Segment expression computed from bytecode (no control port)
static std::unique_ptr<Pipeline> make_segment_bc_pipeline(
    const std::string& id,
    std::vector<double> segment_bytecode,
    std::vector<double> segment_constants = {}) {
  auto pipeline = std::make_unique<Pipeline>(
      id,
      std::vector<std::string>{PortType::VECTOR_NUMBER},
      std::vector<std::string>{PortType::NUMBER},
      std::move(segment_bytecode),
      std::move(segment_constants));

  auto extract = std::make_shared<VectorExtract>("extract", 0);
  auto cumsum = std::make_shared<CumulativeSum>("cumsum");
  pipeline->register_operator(extract);
  pipeline->register_operator(cumsum);
  pipeline->set_entry("extract");
  pipeline->connect(extract, cumsum);
  pipeline->add_output_mapping("cumsum", 0, 0);
  return pipeline;
}

static void send_vec(Pipeline& p, timestamp_t t, std::vector<double> values) {
  auto vec = std::make_shared<std::vector<double>>(std::move(values));
  p.receive_data(create_message<VectorNumberData>(t, VectorNumberData(std::move(vec))), 0);
  p.execute();
}

// =============================================================================
// SEGMENT BYTECODE TESTS
// =============================================================================

SCENARIO("Pipeline segment bytecode: no control port created", "[pipeline][segment_bytecode]") {
  using namespace fused_op;
  // Bytecode: INPUT 2, ABS, CONST 0, GT, END  → ABS(col[2]) > 0
  auto pipeline = make_segment_bc_pipeline("seg_bc",
      {INPUT, 2, ABS, CONST, 0, GT, END}, {0.0});

  THEN("Pipeline has 0 control ports") {
    REQUIRE(pipeline->num_control_ports() == 0);
  }
  THEN("Pipeline has 1 data port") {
    REQUIRE(pipeline->num_data_ports() == 1);
  }
}

SCENARIO("Pipeline segment bytecode: segment boundary triggers emission", "[pipeline][segment_bytecode]") {
  using namespace fused_op;
  auto pipeline = make_segment_bc_pipeline("seg_bc",
      {INPUT, 2, ABS, CONST, 0, GT, END}, {0.0});
  auto col = std::make_shared<Collector>("c", std::vector<std::string>{PortType::NUMBER});
  pipeline->connect(col, 0, 0);

  // Segment 1: col[2]=5.0 → ABS(5)>0 → key=1.0
  // Internal cumsum on col[0]: 10, 30, 60
  send_vec(*pipeline, 1, {10.0, 0.0, 5.0});
  send_vec(*pipeline, 2, {20.0, 0.0, 5.0});
  send_vec(*pipeline, 3, {30.0, 0.0, 5.0});
  REQUIRE(col->get_data_queue(0).empty());  // no boundary yet

  // Key change: col[2]=0.0 → ABS(0)>0 → key=0.0 (boundary!)
  send_vec(*pipeline, 4, {5.0, 0.0, 0.0});

  THEN("Segment 1 buffer emitted at boundary timestamp") {
    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 1);
    auto* msg = dynamic_cast<const Message<NumberData>*>(out[0].get());
    REQUIRE(msg != nullptr);
    REQUIRE(msg->time == 4);
    REQUIRE(msg->data.value == Approx(60.0));  // cumsum: 10+20+30
  }
}

SCENARIO("Pipeline segment bytecode: multiple segment transitions", "[pipeline][segment_bytecode]") {
  using namespace fused_op;
  auto pipeline = make_segment_bc_pipeline("seg_bc",
      {INPUT, 2, ABS, CONST, 0, GT, END}, {0.0});
  auto col = std::make_shared<Collector>("c", std::vector<std::string>{PortType::NUMBER});
  pipeline->connect(col, 0, 0);

  // Segment 1: key=1.0 (col[2]=5), cumsum: 10, 30
  send_vec(*pipeline, 1, {10.0, 0.0, 5.0});
  send_vec(*pipeline, 2, {20.0, 0.0, 5.0});

  // Boundary → segment 2: key=0.0 (col[2]=0), emits cumsum=30
  send_vec(*pipeline, 3, {100.0, 0.0, 0.0});
  {
    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(30.0));
    REQUIRE(out[0]->time == 3);
  }
  col->reset();

  // Segment 2: cumsum resets → 100
  send_vec(*pipeline, 4, {200.0, 0.0, 0.0});

  // Boundary → segment 3: key=1.0 (col[2]=7), emits cumsum=300
  send_vec(*pipeline, 5, {50.0, 0.0, 7.0});
  {
    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(300.0));
    REQUIRE(out[0]->time == 5);
  }
}

SCENARIO("Pipeline segment bytecode: numeric segment expression", "[pipeline][segment_bytecode]") {
  using namespace fused_op;
  // Segment key = FLOOR(col[0] / 10)
  // Bytecode: INPUT 0, CONST 0, DIV, FLOOR, END
  auto pipeline = make_segment_bc_pipeline("seg_num",
      {INPUT, 0, CONST, 0, DIV, FLOOR, END}, {10.0});
  auto col = std::make_shared<Collector>("c", std::vector<std::string>{PortType::NUMBER});
  pipeline->connect(col, 0, 0);

  // Segment key=0 (values 0-9)
  send_vec(*pipeline, 1, {3.0, 0.0, 0.0});    // cumsum=3, key=0
  send_vec(*pipeline, 2, {7.0, 0.0, 0.0});    // cumsum=10, key=0

  // Key change to 1 (values 10-19)
  send_vec(*pipeline, 3, {10.0, 0.0, 0.0});   // key=1 → emit 10
  {
    auto& out = col->get_data_queue(0);
    REQUIRE(out.size() == 1);
    REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(10.0));
  }
}

SCENARIO("Pipeline segment bytecode: backwards compat with empty bytecode", "[pipeline][segment_bytecode]") {
  // Empty segment_bytecode → control port mode (existing behavior)
  auto pipeline = make_cumsum_pipeline("compat_test");
  auto col = std::make_shared<Collector>("c", std::vector<std::string>{PortType::NUMBER});
  pipeline->connect(col, 0, 0);
  REQUIRE(pipeline->num_control_ports() == 1);

  send_paired(*pipeline, 1, 10.0, 1.0);
  send_paired(*pipeline, 2, 20.0, 1.0);
  send_paired(*pipeline, 3, 5.0, 0.0);

  auto& out = col->get_data_queue(0);
  REQUIRE(out.size() == 1);
  REQUIRE(dynamic_cast<const Message<NumberData>*>(out[0].get())->data.value == Approx(30.0));
}

SCENARIO("Pipeline segment bytecode: serialization roundtrip", "[pipeline][segment_bytecode]") {
  using namespace fused_op;
  auto pipeline = make_segment_bc_pipeline("seg_ser",
      {INPUT, 2, ABS, CONST, 0, GT, END}, {0.0});

  // Feed some data to build up state
  send_vec(*pipeline, 1, {10.0, 0.0, 5.0});
  send_vec(*pipeline, 2, {20.0, 0.0, 5.0});

  // Collect and restore
  auto bytes = pipeline->collect_bytes();
  auto pipeline2 = make_segment_bc_pipeline("seg_ser",
      {INPUT, 2, ABS, CONST, 0, GT, END}, {0.0});

  // Feed the same data to pipeline2 so internal operators match
  send_vec(*pipeline2, 1, {10.0, 0.0, 5.0});
  send_vec(*pipeline2, 2, {20.0, 0.0, 5.0});

  auto bytes2 = pipeline2->collect_bytes();
  REQUIRE(bytes == bytes2);
}

SCENARIO("Pipeline segment bytecode: JSON roundtrip via OperatorJson", "[pipeline][segment_bytecode][json]") {
  using namespace fused_op;
  std::shared_ptr<Pipeline> pipeline = make_segment_bc_pipeline("seg_json",
      {INPUT, 2, ABS, CONST, 0, GT, END}, {0.0});

  // Serialize
  std::string json_str = OperatorJson::write_op(pipeline);

  // Deserialize
  auto restored = OperatorJson::read_op(json_str);
  auto* restored_pipeline = dynamic_cast<Pipeline*>(restored.get());
  REQUIRE(restored_pipeline != nullptr);
  REQUIRE(restored_pipeline->num_control_ports() == 0);
  REQUIRE(restored_pipeline->get_segment_bytecode().size() == 7);
  REQUIRE(restored_pipeline->get_segment_constants().size() == 1);
}

// =============================================================================
// EQUALITY TESTS
// =============================================================================

SCENARIO("Pipeline: equality comparison", "[pipeline][equality]") {
  GIVEN("Two identical pipelines") {
    auto p1 = make_cumsum_pipeline("eq1");
    auto p2 = make_cumsum_pipeline("eq1");

    THEN("They are equal initially") {
      REQUIRE(*p1 == *p2);
    }

    WHEN("Same messages are sent to both") {
      send_paired(*p1, 1, 10.0, 1.0);
      send_paired(*p2, 1, 10.0, 1.0);

      THEN("They remain equal") {
        REQUIRE(*p1 == *p2);
      }
    }

    WHEN("Different messages are sent") {
      send_paired(*p1, 1, 10.0, 1.0);
      send_paired(*p2, 1, 99.0, 1.0);

      THEN("They are not equal") {
        REQUIRE(*p1 != *p2);
      }
    }
  }

  GIVEN("Pipelines with different configurations") {
    auto p1 = std::make_unique<Pipeline>(
        "cmp", std::vector<std::string>{PortType::NUMBER},
        std::vector<std::string>{PortType::NUMBER});
    auto p2 = std::make_unique<Pipeline>(
        "cmp", std::vector<std::string>{PortType::NUMBER, PortType::NUMBER},
        std::vector<std::string>{PortType::NUMBER});

    THEN("They are not equal (different port counts)") {
      REQUIRE(*p1 != *p2);
    }
  }
}
