#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/Program.h"
#include "rtbot/bindings.h"

using namespace rtbot;

SCENARIO("Program handles basic operator configurations", "[program]") {
  GIVEN("A simple program with one input and one output") {
    std::string program_json = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program program(program_json);

    WHEN("Receiving a message") {
      Message<NumberData> msg(1, NumberData{42.0});
      auto batch = program.receive(msg);

      THEN("Message is propagated to output") {
        REQUIRE(batch.size() == 1);
        REQUIRE(batch.count("output1") == 1);
        REQUIRE(batch["output1"].count("o1") == 1);

        const auto& msgs = batch["output1"]["o1"];
        REQUIRE(msgs.size() == 1);

        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(msgs[0].get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 1);
        REQUIRE(out_msg->data.value == 42.0);
      }
    }
  }

  GIVEN("A program with multiple operators") {
    std::string program_json = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
        {"from": "ma1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program program(program_json);

    WHEN("Processing multiple messages") {
      std::vector<Message<NumberData>> messages = {{1, NumberData{3.0}}, {2, NumberData{6.0}}, {3, NumberData{9.0}}};

      ProgramMsgBatch final_batch;
      for (const auto& msg : messages) {
        final_batch = program.receive(msg);
      }

      THEN("Moving average is calculated correctly") {
        REQUIRE(final_batch.size() == 1);
        REQUIRE(final_batch["output1"]["o1"].size() == 1);

        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 3);
        REQUIRE(out_msg->data.value == Approx(6.0));
      }
    }
  }
}

SCENARIO("Program handles serialization and deserialization", "[program]") {
  GIVEN("A program with state") {
    std::string program_json = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
        {"from": "ma1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program original_program(program_json);

    // Feed some data to establish state
    original_program.receive(Message<NumberData>(1, NumberData{3.0}));
    original_program.receive(Message<NumberData>(2, NumberData{6.0}));

    WHEN("Serializing and deserializing") {
      auto serialized = original_program.serialize_data();
      Program restored_program(program_json);
      restored_program.restore_data_from_json(serialized);

      THEN("State is preserved") {
        auto original_batch = original_program.receive(Message<NumberData>(3, NumberData{9.0}));
        auto restored_batch = restored_program.receive(Message<NumberData>(3, NumberData{9.0}));

        const auto* original_msg =
            dynamic_cast<const Message<NumberData>*>(original_batch["output1"]["o1"].back().get());
        const auto* restored_msg =
            dynamic_cast<const Message<NumberData>*>(restored_batch["output1"]["o1"].back().get());

        REQUIRE(original_msg->data.value == Approx(restored_msg->data.value));
      }
    }
  }
}

SCENARIO("Program Manager handles multiple programs", "[program_manager]") {
  GIVEN("A program manager with multiple programs") {
    auto& manager = ProgramManager::instance();
    manager.clear_all_programs();  // Reset state before test

    std::string program1_json = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    REQUIRE(manager.create_program("prog1", program1_json) == "");

    WHEN("Processing messages through multiple programs") {
      Message<NumberData> msg(1, NumberData{42.0});
      REQUIRE(manager.add_to_message_buffer("prog1", "i1", msg));

      auto batch = manager.process_message_buffer("prog1");

      THEN("Messages are processed independently") {
        REQUIRE(batch.size() == 1);
        REQUIRE(batch["output1"]["o1"].size() == 1);

        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(batch["output1"]["o1"][0].get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->data.value == 42.0);
      }
    }

    WHEN("Deleting a program") {
      REQUIRE(manager.delete_program("prog1"));
      THEN("Program cannot be accessed") {
        REQUIRE_FALSE(manager.add_to_message_buffer("prog1", "i1", Message<NumberData>(1, NumberData{42.0})));
      }
    }
  }
}

SCENARIO("Program handles debug mode", "[program]") {
  GIVEN("A program with multiple operators") {
    std::string program_json = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
        {"from": "ma1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program program(program_json);

    WHEN("Processing in debug mode") {
      program.receive_debug(Message<NumberData>(1, NumberData{42.0}));
      program.receive_debug(Message<NumberData>(2, NumberData{142.0}));
      auto batch = program.receive_debug(Message<NumberData>(3, NumberData{242.0}));

      THEN("All operator outputs are captured") {
        REQUIRE(batch.size() > 1);
        REQUIRE(batch.count("input1") == 1);
        REQUIRE(batch.count("ma1") == 1);
        REQUIRE(batch.count("output1") == 1);
      }
    }
  }
}

SCENARIO("Program handles Pipeline operators", "[program][pipeline]") {
  GIVEN("A program with a Pipeline containing multiple connected operators") {
    // Pipeline requires a control port signal for segment-scoped computation.
    // Input has 2 ports: o1 → pipeline data, o2 → pipeline control.
    // Key changes drive emission: stable key accumulates, different key flushes.
    std::string program_json = R"({
            "operators": [
                {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
                {
                    "type": "Pipeline",
                    "id": "pipeline1",
                    "input_port_types": ["number"],
                    "output_port_types": ["number"],
                    "operators": [
                        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
                        {"type": "StandardDeviation", "id": "std1", "window_size": 3}
                    ],
                    "connections": [
                        {"from": "ma1", "to": "std1", "fromPort": "o1", "toPort": "i1"}
                    ],
                    "entryOperator": "ma1",
                    "outputMappings": {
                        "std1": {"o1": "o1"}
                    }
                },
                {"type": "Output", "id": "output1", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input1", "to": "pipeline1", "fromPort": "o1", "toPort": "i1"},
                {"from": "input1", "to": "pipeline1", "fromPort": "o2", "toPort": "i1", "toPortType": "control"},
                {"from": "pipeline1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input1",
            "output": {
                "output1": ["o1"]
            }
        })";

    Program program(program_json);

    WHEN("Processing messages with key change to trigger emission") {
      // MA(3) needs 3 messages to produce first output.
      // STD(3) needs 3 MA outputs → 5 data messages total.
      // All with key=1.0 (same segment). Then key=2.0 triggers flush.
      ProgramMsgBatch final_batch;

      // Send 5 data+control pairs with stable key=1.0
      for (int t = 1; t <= 5; ++t) {
        program.receive({t, NumberData{static_cast<double>(t * 3)}}, "i1");
        final_batch = program.receive({t, NumberData{1.0}}, "i2");
      }

      THEN("No output yet (key has not changed)") {
        REQUIRE(final_batch.empty());
      }

      AND_WHEN("Key changes to trigger emission") {
        // Send one more pair with key=2.0 → key change → emit buffered STD output
        program.receive({6, NumberData{18.0}}, "i1");
        final_batch = program.receive({6, NumberData{2.0}}, "i2");

        THEN("Pipeline emits buffered output at boundary timestamp") {
          REQUIRE(final_batch.size() == 1);
          REQUIRE(final_batch["output1"].count("o1") == 1);
          const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
          REQUIRE(out_msg != nullptr);
          REQUIRE(out_msg->time == 6);  // boundary timestamp
        }
      }
    }

    WHEN("Processing fewer messages (not enough for internal output)") {
      // Only 4 data messages — STD(3) needs 3 MA outputs but only gets 2
      ProgramMsgBatch final_batch;

      for (int t = 1; t <= 4; ++t) {
        program.receive({t, NumberData{static_cast<double>(t * 3)}}, "i1");
        final_batch = program.receive({t, NumberData{1.0}}, "i2");
      }

      // Key change — but nothing buffered since STD never produced output
      program.receive({5, NumberData{15.0}}, "i1");
      final_batch = program.receive({5, NumberData{2.0}}, "i2");

      THEN("No output is produced (internal operators never produced output)") {
        REQUIRE(final_batch.empty());
      }
    }
  }

  GIVEN("A program with an invalid Pipeline configuration") {
    std::string invalid_program = R"({
            "operators": [
                {"type": "Input", "id": "input1", "portTypes": ["number"]},
                {
                    "type": "Pipeline",
                    "id": "pipeline1",
                    "input_port_types": ["number"],
                    "output_port_types": ["number"],
                    "operators": [
                        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
                        {"type": "MovingAverage", "id": "ma2", "window_size": 2}
                    ],
                    "connections": [{
                        "from": "ma1",
                        "to": "ma2",
                        "fromPort": "o1",
                        "toPort": "i1"
                    }],
                    "entryOperator": "nonexistent",
                    "outputMappings": {
                        "ma1": {"o1": "o1"}
                    }
                },
                {"type": "Output", "id": "output1", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input1", "to": "pipeline1", "fromPort": "o1", "toPort": "i1"},
                {"from": "pipeline1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input1",
            "output": {
                "output1": ["o1"]
            }
        })";

    THEN("Program creation fails with appropriate error") {
      REQUIRE_THROWS_WITH(Program(invalid_program), Catch::Contains("Entry operator not found"));
    }
  }
}

SCENARIO("Program handles Pipeline operators and resets", "[program][pipeline]") {
  GIVEN("A program with a Pipeline") {
    // Pipeline with control port for segment-scoped computation.
    // Input has 2 ports: o1 → data, o2 → control.
    // Key changes trigger emission + reset of internal operators.
    std::string program_json = R"({
            "operators": [
                {"type": "Input", "id": "input", "portTypes": ["number", "number"]},
                {
                    "type": "Pipeline",
                    "id": "pipeline",
                    "input_port_types": ["number"],
                    "output_port_types": ["number"],
                    "operators": [
                        {"type": "Input", "id": "pinput", "portTypes": ["number"]},
                        {"type": "MovingAverage", "id": "ma", "window_size": 3}
                    ],
                    "connections": [
                        {"from": "pinput", "to": "ma", "fromPort": "o1", "toPort": "i1"}
                    ],
                    "entryOperator": "pinput",
                    "outputMappings": {
                        "ma": {"o1": "o1"}
                    }
                },
                {"type": "Output", "id": "output", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input", "to": "pipeline", "fromPort": "o1", "toPort": "i1"},
                {"from": "input", "to": "pipeline", "fromPort": "o2", "toPort": "i1", "toPortType": "control"},
                {"from": "pipeline", "to": "output", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input",
            "output": {
                "output": ["o1"]
            }
        })";

    Program program(program_json);

    WHEN("Processing messages across multiple segments") {
      // Segment 1 (key=1.0): 3 messages fill MA(3), MA produces output, buffered
      // Key change to 2.0: emit + reset
      // Segment 2 (key=2.0): 3 messages fill MA(3) again, MA produces output, buffered
      // Key change to 3.0: emit + reset
      // Segment 3 (key=3.0): 3 more messages, same pattern
      // Key change to 4.0: emit + reset
      ProgramMsgBatch final_batch;

      // --- Segment 1: key=1.0, data={1,2,3} ---
      program.receive({1, NumberData{1.0}}, "i1");
      final_batch = program.receive({1, NumberData{1.0}}, "i2");
      REQUIRE(final_batch.empty());

      program.receive({2, NumberData{2.0}}, "i1");
      final_batch = program.receive({2, NumberData{1.0}}, "i2");
      REQUIRE(final_batch.empty());

      program.receive({3, NumberData{3.0}}, "i1");
      final_batch = program.receive({3, NumberData{1.0}}, "i2");
      // MA(3) emits avg(1,2,3)=2.0 but no key change yet → still buffered
      REQUIRE(final_batch.empty());

      // --- Key change to 2.0: triggers emission of segment 1 buffer ---
      program.receive({4, NumberData{4.0}}, "i1");
      final_batch = program.receive({4, NumberData{2.0}}, "i2");

      THEN("Pipeline emits segment 1 output at boundary timestamp") {
        REQUIRE(final_batch.size() == 1);
        REQUIRE(final_batch["output"].count("o1") == 1);
        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 4);    // boundary timestamp
        REQUIRE(out_msg->data.value == 2.0);  // avg(1,2,3)

        // --- Segment 2: key=2.0, data={4,5,6} (MA was reset) ---
        program.receive({5, NumberData{5.0}}, "i1");
        final_batch = program.receive({5, NumberData{2.0}}, "i2");

        AND_THEN("Pipeline collects after reset") {
          REQUIRE(final_batch.empty());

          program.receive({6, NumberData{6.0}}, "i1");
          final_batch = program.receive({6, NumberData{2.0}}, "i2");
          // MA(3) emits avg(4,5,6)=5.0, buffered
          REQUIRE(final_batch.empty());

          // Key change to 3.0: emit segment 2
          program.receive({7, NumberData{0.0}}, "i1");
          final_batch = program.receive({7, NumberData{3.0}}, "i2");

          AND_THEN("Pipeline emits segment 2 output") {
            REQUIRE(final_batch.size() == 1);
            REQUIRE(final_batch["output"].count("o1") == 1);
            const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output"]["o1"].back().get());
            REQUIRE(out_msg != nullptr);
            REQUIRE(out_msg->time == 7);
            REQUIRE(out_msg->data.value == 5.0);  // avg(4,5,6)

            // --- Segment 3: key=3.0, data={0,0,0} (MA reset again) ---
            program.receive({8, NumberData{0.0}}, "i1");
            final_batch = program.receive({8, NumberData{3.0}}, "i2");

            program.receive({9, NumberData{0.0}}, "i1");
            final_batch = program.receive({9, NumberData{3.0}}, "i2");
            // MA(3) emits avg(0,0,0)=0.0, buffered

            // Key change to 4.0: emit segment 3
            program.receive({10, NumberData{99.0}}, "i1");
            final_batch = program.receive({10, NumberData{4.0}}, "i2");

            AND_THEN("Pipeline emits segment 3 output") {
              REQUIRE(final_batch.size() == 1);
              REQUIRE(final_batch["output"].count("o1") == 1);
              const auto* out_msg =
                  dynamic_cast<const Message<NumberData>*>(final_batch["output"]["o1"].back().get());
              REQUIRE(out_msg != nullptr);
              REQUIRE(out_msg->time == 10);
              REQUIRE(out_msg->data.value == 0.0);  // avg(0,0,0)
            }
          }
        }
      }
    }
  }
}

SCENARIO("Program handles complex Pipeline operators and resets", "[program][pipeline]") {
  GIVEN("A complex program with a Pipeline") {
    // Power consumption monitor: two current inputs → scale to power → sum → Pipeline (accumulate energy).
    // Pipeline requires control port for segment-scoped computation.
    // Input port 3 (o3) feeds the Pipeline's control port directly.
    // Key changes (segment index) trigger emission of accumulated energy value.
    std::string program_json = R"({
    "apiVersion": "v1",
    "operators": [
        {
            "id": "input",
            "type": "Input",
            "portTypes": [
                "number",
                "number",
                "number"
            ]
        },
        {
            "id": "hi_input_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "lo_input_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "hiresampler",
            "type": "ResamplerConstant",
            "interval": 5000
        },
        {
            "id": "loresampler",
            "type": "ResamplerConstant",
            "interval": 5000
        },
        {
            "id": "hiresampler_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },        
        {
            "id": "loresampler_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "power1",
            "type": "Scale",
            "value": 220
        },
        {
            "id": "power2",
            "type": "Scale",
            "value": 220
        },
        {
            "id": "power1_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "power2_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "total_power",
            "type": "Addition"
        },
        {
            "id": "total_power_cutoff",
            "type": "LessThanOrEqualToReplace",
            "value": 0.5,
            "replaceBy": 0.0
        },
        {
            "id": "hourly",
            "type": "Pipeline",
            "input_port_types": [
                "number"
            ],
            "output_port_types": [
                "number"
            ],
            "operators": [                
                {
                    "id": "trapezoid",
                    "type": "MovingAverage",
                    "window_size": 2
                },
                {
                    "id": "trapezoid_cutoff",
                    "type": "LessThanOrEqualToReplace",
                    "value": 0.5,
                    "replaceBy": 0.0
                },
                {
                    "id": "wh",
                    "type": "Scale",
                    "value": 0.00138888888
                },
                {
                    "id": "wh_cutoff",
                    "type": "LessThanOrEqualToReplace",
                    "value": 0.5,
                    "replaceBy": 0.0
                },                
                {
                    "id": "hourly_ms",
                    "type": "MovingSum",
                    "window_size": 10
                }
            ],
            "connections": [              
                {
                    "from": "trapezoid",
                    "to": "trapezoid_cutoff"
                },
                {
                    "from": "trapezoid_cutoff",
                    "to": "wh"
                },
                {
                    "from": "wh",
                    "to": "wh_cutoff"
                },
                {
                    "from": "wh_cutoff",
                    "to": "hourly_ms"
                }
            ],
            "entryOperator": "trapezoid",
            "outputMappings": {
                "hourly_ms": {
                    "o1": "o1"
                }
            }
        },
        {
            "id": "output",
            "type": "Output",
            "portTypes": [
                "number"                
            ]
        }
    ],
    "connections": [
        {
            "from": "input",
            "to": "hi_input_cutoff",
            "fromPort": "o1",
            "toPort": "i1"           
        },
        {
            "from": "input",
            "to": "lo_input_cutoff",
            "fromPort": "o2",
            "toPort": "i1"
        },
        {
            "from": "hi_input_cutoff",
            "to": "hiresampler"
        },
        {
            "from": "lo_input_cutoff",
            "to": "loresampler"
        },
        {
            "from": "hiresampler",
            "to": "hiresampler_cutoff"
        },
        {
            "from": "hiresampler_cutoff",
            "to": "power1"
        },        
        {
            "from": "loresampler",
            "to": "loresampler_cutoff"
        },
        {
            "from": "loresampler_cutoff",
            "to": "power2"
        },
        {
            "from": "power1",
            "to": "power1_cutoff"
        },
        {
            "from": "power2",
            "to": "power2_cutoff"
        },        
        {
            "from": "power1_cutoff",
            "to": "total_power",
            "fromPort": "o1",
            "toPort": "i1"
        },
        {
            "from": "power2_cutoff",
            "to": "total_power",
            "fromPort": "o1",
            "toPort": "i2"
        },
        {
            "from": "total_power",
            "to": "total_power_cutoff"
        },
        {
            "from": "total_power_cutoff",
            "to": "hourly"
        },
        {
            "from": "input",
            "to": "hourly",
            "fromPort": "o3",
            "toPort": "i1",
            "toPortType": "control"
        },
        {
            "from": "hourly",
            "to": "output",
            "fromPort": "o1",
            "toPort": "i1"
        }
    ],
    "entryOperator": "input",
    "output": {
        "output": [
            "o1"
        ]
    },
    "title": "Power Consumption Monitor with Multiple Averages",
    "description": "Calculates hourly power consumption from two current inputs"
})";

    Program program(program_json);

    WHEN("Processing messages across segments") {
      // Each segment: send N messages with same key (high or low power),
      // then change key to trigger emission.
      // Data flows: input → cutoffs → resamplers → scales → addition → cutoff → hourly Pipeline
      // Control flows: input:o3 → hourly:control (timestamps must match data arriving at Pipeline)
      //
      // With interval=5000 and data sent at 5000ms intervals, data arrives at Pipeline
      // at the same timestamps as control. MovingSum(10) fills after 10 inputs.
      // We send 15 messages per segment to ensure MovingSum produces output.
      int t = 5000;
      int messages_per_segment = 15;

      for (int h = 1; h <= 4; h++) {
        double hi_current = (h % 2 == 1) ? 30.23 : 0.01;
        double lo_current = (h % 2 == 1) ? 10.5802 : 0.01;
        double key = static_cast<double>(h);

        for (int i = 0; i < messages_per_segment; ++i) {
          program.receive({t, NumberData{hi_current}}, "i1");
          program.receive({t, NumberData{lo_current}}, "i2");
          program.receive({t, NumberData{key}}, "i3");
          t += 5000;
        }
      }

      // Send one final message with a new key to flush the last segment
      program.receive({t, NumberData{0.01}}, "i1");
      program.receive({t, NumberData{0.01}}, "i2");
      ProgramMsgBatch final_batch = program.receive({t, NumberData{5.0}}, "i3");

      THEN("Pipeline produces output from accumulated segments") {
        // The final key change should trigger emission of segment 4's buffer.
        // Earlier segments emitted when key changed from h to h+1.
        // We verify the output exists and check value sign for the last segment.
        REQUIRE(final_batch.size() == 1);
        REQUIRE(final_batch["output"].count("o1") == 1);
        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        // Segment 4 is even (low power) → output should be 0.0 (cutoff replaces ≤0.5 with 0.0)
        REQUIRE(out_msg->data.value == 0.0);
      }
    }
  }
}

SCENARIO("Program handles Pipeline serialization", "[program][pipeline]") {
  GIVEN("A program with a stateful Pipeline and control port") {
    // Pipeline with MA1(3)→MA2(2), control port for segment-scoped computation.
    // Input has 2 ports: o1 → pipeline data, o2 → pipeline control.
    std::string program_json = R"({
            "operators": [
                {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
                {
                    "type": "Pipeline",
                    "id": "pipeline1",
                    "input_port_types": ["number"],
                    "output_port_types": ["number"],
                    "operators": [
                        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
                        {"type": "MovingAverage", "id": "ma2", "window_size": 2}
                    ],
                    "connections": [
                        {"from": "ma1", "to": "ma2", "fromPort": "o1", "toPort": "i1"}
                    ],
                    "entryOperator": "ma1",
                    "outputMappings": {
                        "ma2": {"o1": "o1"}
                    }
                },
                {"type": "Output", "id": "output1", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input1", "to": "pipeline1", "fromPort": "o1", "toPort": "i1"},
                {"from": "input1", "to": "pipeline1", "fromPort": "o2", "toPort": "i1", "toPortType": "control"},
                {"from": "pipeline1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input1",
            "output": {
                "output1": ["o1"]
            }
        })";

    Program original_program(program_json);

    WHEN("Processing messages and serializing at critical state") {
      // MA1(3)→MA2(2) with key=1.0
      // t=1: MA1 collecting
      // t=2: MA1 collecting
      // t=3: MA1 emits avg(10,20,30)=20 → MA2 collecting
      // t=4: MA1 emits avg(20,30,40)=30 → MA2 emits avg(20,30)=25 → buffered
      ProgramMsgBatch batch;
      original_program.receive({1, NumberData{10.0}}, "i1");
      batch = original_program.receive({1, NumberData{1.0}}, "i2");
      REQUIRE(batch.empty());

      original_program.receive({2, NumberData{20.0}}, "i1");
      batch = original_program.receive({2, NumberData{1.0}}, "i2");
      REQUIRE(batch.empty());

      original_program.receive({3, NumberData{30.0}}, "i1");
      batch = original_program.receive({3, NumberData{1.0}}, "i2");
      REQUIRE(batch.empty());

      original_program.receive({4, NumberData{40.0}}, "i1");
      batch = original_program.receive({4, NumberData{1.0}}, "i2");
      // MA2 has emitted, output buffered but key hasn't changed
      REQUIRE(batch.empty());

      // Serialize at critical state (buffer has MA2 output = 25.0)
      auto serialized = original_program.serialize_data();
      Program restored_program(program_json);
      restored_program.restore_data_from_json(serialized);

      // Key change to 2.0 on both programs → emit buffer
      original_program.receive({5, NumberData{50.0}}, "i1");
      auto original_batch = original_program.receive({5, NumberData{2.0}}, "i2");

      restored_program.receive({5, NumberData{50.0}}, "i1");
      auto restored_batch = restored_program.receive({5, NumberData{2.0}}, "i2");

      THEN("Both programs produce identical first output") {
        REQUIRE(original_batch.count("output1") == 1);
        REQUIRE(restored_batch.count("output1") == 1);
        REQUIRE(original_batch["output1"].count("o1") == 1);
        REQUIRE(restored_batch["output1"].count("o1") == 1);

        const auto* original_msg =
            dynamic_cast<const Message<NumberData>*>(original_batch["output1"]["o1"].back().get());
        const auto* restored_msg =
            dynamic_cast<const Message<NumberData>*>(restored_batch["output1"]["o1"].back().get());

        REQUIRE(original_msg != nullptr);
        REQUIRE(restored_msg != nullptr);
        REQUIRE(original_msg->time == 5);   // boundary timestamp
        REQUIRE(restored_msg->time == 5);

        // Both should output 25.0 (MA2 avg of first two MA1 outputs: 20,30)
        REQUIRE(original_msg->data.value == Approx(25.0));
        REQUIRE(restored_msg->data.value == Approx(25.0));
      }
    }
  }
}

SCENARIO("Pipeline reset and emission behavior", "[program][pipeline]") {
  GIVEN("A pipeline that requires state reset after emission") {
    // Pipeline with MA1(3)→MA2(2), control port for segment-scoped computation.
    // Input has 2 ports: o1 → pipeline data, o2 → pipeline control.
    std::string program_json = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
        {
          "type": "Pipeline",
          "id": "pipeline1",
          "input_port_types": ["number"],
          "output_port_types": ["number"],
          "operators": [
            {"type": "MovingAverage", "id": "ma1", "window_size": 3},
            {"type": "MovingAverage", "id": "ma2", "window_size": 2}
          ],
          "connections": [
            {"from": "ma1", "to": "ma2", "fromPort": "o1", "toPort": "i1"}
          ],
          "entryOperator": "ma1",
          "outputMappings": {
            "ma2": {"o1": "o1"}
          }
        },
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "pipeline1", "fromPort": "o1", "toPort": "i1"},
        {"from": "input1", "to": "pipeline1", "fromPort": "o2", "toPort": "i1", "toPortType": "control"},
        {"from": "pipeline1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program original_program(program_json);

    WHEN("Processing messages before and after serialization") {
      ProgramMsgBatch last_batch;

      // Segment 1 (key=1.0): 4 messages to get MA1(3)→MA2(2) to produce output
      // t=1: MA1 collecting
      // t=2: MA1 collecting
      // t=3: MA1 emits avg(10,20,30)=20 → MA2 collecting
      // t=4: MA1 emits avg(20,30,40)=30 → MA2 emits avg(20,30)=25 → buffered
      for (int t = 1; t <= 4; ++t) {
        original_program.receive({(int64_t)t, NumberData{t * 10.0}}, "i1");
        last_batch = original_program.receive({(int64_t)t, NumberData{1.0}}, "i2");
      }
      // No output yet (key hasn't changed)
      REQUIRE(last_batch.empty());

      // Key change to 2.0 → emit segment 1 buffer at boundary t=5
      original_program.receive({5, NumberData{50.0}}, "i1");
      last_batch = original_program.receive({5, NumberData{2.0}}, "i2");

      // Verify first output
      REQUIRE(last_batch.count("output1") == 1);
      REQUIRE(last_batch["output1"].count("o1") == 1);

      const auto* first_output = dynamic_cast<const Message<NumberData>*>(last_batch["output1"]["o1"].back().get());
      REQUIRE(first_output != nullptr);
      INFO("First output value: " << first_output->data.value);
      REQUIRE(first_output->data.value == Approx(25.0));  // avg(20,30) from MA2
      REQUIRE(first_output->time == 5);  // boundary timestamp

      // Segment 2 (key=2.0): internals were reset, 4 more messages
      // t=6: MA1 collecting (fresh)
      // t=7: MA1 collecting
      // t=8: MA1 emits avg(60,70,80)=70 → MA2 collecting
      // Note: t=5 data (50.0) is part of segment 2 since key=2.0 started there
      // After key change at t=5, forward_and_buffer runs with data=50 + key=2.0
      // So MA1 has: [50] at t=5
      // t=6: MA1 has [50,60]
      // t=7: MA1 has [50,60,70] → emits avg=60 → MA2 gets [60]
      // t=8: MA1 has [60,70,80] → emits avg=70 → MA2 gets [60,70] → emits avg=65 → buffered
      for (int t = 6; t <= 8; ++t) {
        original_program.receive({(int64_t)t, NumberData{t * 10.0}}, "i1");
        last_batch = original_program.receive({(int64_t)t, NumberData{2.0}}, "i2");
      }
      // No output (key stable)
      REQUIRE(last_batch.empty());

      // Key change to 3.0 → emit segment 2 buffer at boundary t=9
      original_program.receive({9, NumberData{90.0}}, "i1");
      last_batch = original_program.receive({9, NumberData{3.0}}, "i2");

      REQUIRE(last_batch.count("output1") == 1);
      const auto* second_output = dynamic_cast<const Message<NumberData>*>(last_batch["output1"]["o1"].back().get());
      REQUIRE(second_output != nullptr);
      INFO("Second output value: " << second_output->data.value);
      REQUIRE(second_output->data.value == Approx(65.0));  // avg(60,70) from MA2
      REQUIRE(second_output->time == 9);

      // Serialize and restore
      auto serialized = original_program.serialize_data();
      Program restored_program(program_json);
      restored_program.restore_data_from_json(serialized);

      // Segment 3 (key=3.0): both programs start fresh after reset
      // t=9 data (90.0) is part of segment 3
      // t=10: MA1 has [90,100]
      // t=11: MA1 has [90,100,110] → emits avg=100 → MA2 gets [100]
      // t=12: MA1 has [100,110,120] → emits avg=110 → MA2 gets [100,110] → emits avg=105 → buffered
      for (int t = 10; t <= 12; ++t) {
        original_program.receive({(int64_t)t, NumberData{t * 10.0}}, "i1");
        original_program.receive({(int64_t)t, NumberData{3.0}}, "i2");

        restored_program.receive({(int64_t)t, NumberData{t * 10.0}}, "i1");
        restored_program.receive({(int64_t)t, NumberData{3.0}}, "i2");
      }

      // Key change to 4.0 → emit segment 3 at boundary t=13
      original_program.receive({13, NumberData{130.0}}, "i1");
      auto original_batch = original_program.receive({13, NumberData{4.0}}, "i2");

      restored_program.receive({13, NumberData{130.0}}, "i1");
      auto restored_batch = restored_program.receive({13, NumberData{4.0}}, "i2");

      THEN("Both programs maintain correct reset behavior") {
        REQUIRE(original_batch.count("output1") == 1);
        REQUIRE(restored_batch.count("output1") == 1);
        REQUIRE(original_batch["output1"].count("o1") == 1);
        REQUIRE(restored_batch["output1"].count("o1") == 1);

        const auto* original_msg =
            dynamic_cast<const Message<NumberData>*>(original_batch["output1"]["o1"].back().get());
        const auto* restored_msg =
            dynamic_cast<const Message<NumberData>*>(restored_batch["output1"]["o1"].back().get());

        REQUIRE(original_msg != nullptr);
        REQUIRE(restored_msg != nullptr);

        INFO("Original program final value: " << original_msg->data.value);
        INFO("Restored program final value: " << restored_msg->data.value);

        // Both programs should output 105 (avg of MA1 outputs 100,110 through MA2)
        REQUIRE(original_msg->data.value == Approx(105.0));
        REQUIRE(restored_msg->data.value == Approx(105.0));
        REQUIRE(original_msg->time == 13);
        REQUIRE(restored_msg->time == 13);
      }
    }
  }
}

SCENARIO("Program handles prototypes", "[program][prototypes]") {
  GIVEN("A program with a simple prototype") {
    std::string program_json = R"({
      "prototypes": {
        "adjustableMA": {
          "parameters": [
            {"name": "window", "type": "number"},
            {"name": "scale", "type": "number", "default": 1.0}
          ],
          "operators": [
            {"type": "MovingAverage", "id": "ma", "window_size": "${window}"},
            {"type": "Scale", "id": "scale", "value": "${scale}"}
          ],
          "connections": [
            {"from": "ma", "to": "scale"}
          ],
          "entry": {"operator": "ma"},
          "output": {"operator": "scale"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"id": "ma_instance", "prototype": "adjustableMA", "parameters": {"window": 3, "scale": 2.0}},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "ma_instance"},
        {"from": "ma_instance", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program program(program_json);

    WHEN("Processing messages") {
      std::vector<Message<NumberData>> messages = {{1, NumberData{3.0}}, {2, NumberData{6.0}}, {3, NumberData{9.0}}};

      ProgramMsgBatch final_batch;
      for (const auto& msg : messages) {
        final_batch = program.receive(msg);
      }

      THEN("Output reflects both MA and scaling") {
        REQUIRE(final_batch.size() == 1);
        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->data.value == Approx(12.0));  // MA(3,6,9)=6.0, then scaled by 2.0
      }
    }

    WHEN("Serializing and deserializing") {
      program.receive(Message<NumberData>(1, NumberData{3.0}));
      program.receive(Message<NumberData>(2, NumberData{6.0}));

      auto serialized = program.serialize_data();
      Program restored(program_json);
      restored.restore_data_from_json(serialized);

      auto original_batch = program.receive(Message<NumberData>(3, NumberData{9.0}));
      auto restored_batch = restored.receive(Message<NumberData>(3, NumberData{9.0}));

      THEN("State is preserved correctly") {
        const auto* original_msg =
            dynamic_cast<const Message<NumberData>*>(original_batch["output1"]["o1"].back().get());
        const auto* restored_msg =
            dynamic_cast<const Message<NumberData>*>(restored_batch["output1"]["o1"].back().get());
        REQUIRE(original_msg->data.value == Approx(restored_msg->data.value));
      }
    }
  }

  GIVEN("Invalid prototype configurations") {
    WHEN("Missing required parameter") {
      std::string invalid_json = R"({
        "prototypes": {
          "adjustableMA": {
            "parameters": [
              {"name": "window", "type": "number"}
            ],
            "operators": [
              {"type": "MovingAverage", "id": "ma", "window_size": "${window}"}
            ],
            "connections": [],
            "entry": {"operator": "ma"},
            "output": {"operator": "ma"}
          }
        },
        "operators": [
          {"type": "Input", "id": "input1", "portTypes": ["number"]},
          {"id": "ma_instance", "prototype": "adjustableMA"},
          {"type": "Output", "id": "output1", "portTypes": ["number"]}
        ],
        "connections": [
          {"from": "input1", "to": "ma_instance"},
          {"from": "ma_instance", "to": "output1"}
        ],
        "entryOperator": "input1",
        "output": {"output1": ["o1"]}
      })";

      THEN("Program creation fails") {
        REQUIRE_THROWS_WITH(Program(invalid_json), Catch::Contains("Missing required parameter 'window'"));
      }
    }

    WHEN("Invalid parameter type") {
      std::string invalid_json = R"({
        "prototypes": {
          "adjustableMA": {
            "parameters": [
              {"name": "window", "type": "number"}
            ],
            "operators": [
              {"type": "MovingAverage", "id": "ma", "window_size": "${window}"}
            ],
            "connections": [],
            "entry": {"operator": "ma"},
            "output": {"operator": "ma"}
          }
        },
        "operators": [
          {"type": "Input", "id": "input1", "portTypes": ["number"]},
          {"id": "ma_instance", "prototype": "adjustableMA", 
           "parameters": {"window": "not_a_number"}},
          {"type": "Output", "id": "output1", "portTypes": ["number"]}
        ],
        "connections": [
          {"from": "input1", "to": "ma_instance"},
          {"from": "ma_instance", "to": "output1"}
        ],
        "entryOperator": "input1",
        "output": {"output1": ["o1"]}
      })";

      THEN("Program creation fails") {
        REQUIRE_THROWS_WITH(Program(invalid_json), Catch::Contains("Parameter 'window' must be a number"));
      }
    }
  }
}

SCENARIO("Program handles Pipeline prototypes", "[program][prototypes][pipeline]") {
  GIVEN("A program with a prototype containing a Pipeline") {
    // The prototype has a Pipeline with MA(3)→STD(3) inside.
    // Pipeline requires control port for segment-scoped computation.
    // Input has 2 ports: o1 → data (goes through prototype entry → pipeline data),
    //                    o2 → control (goes through prototype entry → pipeline control).
    std::string program_json = R"({
      "prototypes": {
        "complexProcessor": {
          "parameters": [
            {"name": "ma_window", "type": "number"},
            {"name": "std_window", "type": "number", "default": 3}
          ],
          "operators": [
            {
              "type": "Pipeline",
              "id": "pipeline",
              "input_port_types": ["number"],
              "output_port_types": ["number"],
              "operators": [
                {"type": "MovingAverage", "id": "ma", "window_size": "${ma_window}"},
                {"type": "StandardDeviation", "id": "std", "window_size": "${std_window}"}
              ],
              "connections": [
                {"from": "ma", "to": "std", "fromPort": "o1", "toPort": "i1"}
              ],
              "entryOperator": "ma",
              "outputMappings": {
                "std": {"o1": "o1"}
              }
            }
          ],
          "connections": [],
          "entry": {"operator": "pipeline"},
          "output": {"operator": "pipeline"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
        {
          "id": "proc1",
          "prototype": "complexProcessor",
          "parameters": {"ma_window": 3}
        },
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "proc1", "fromPort": "o1", "toPort": "i1"},
        {"from": "input1", "to": "proc1", "fromPort": "o2", "toPort": "i1", "toPortType": "control"},
        {"from": "proc1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program program(program_json);

    WHEN("Processing messages") {
      // MA(3)→STD(3) inside Pipeline with control port.
      // Need 5 data+control pairs with stable key to get MA→STD to produce output,
      // then key change to trigger emission.
      //
      // t=1: MA collecting, key=1
      // t=2: MA collecting, key=1
      // t=3: MA emits avg(3,6,9)=6 → STD collecting, key=1
      // t=4: MA emits avg(6,9,12)=9 → STD collecting, key=1
      // t=5: MA emits avg(9,12,15)=12 → STD emits std(6,9,12)=3.0 → buffered, key=1
      // t=6: key change to 2 → emit buffer at t=6

      ProgramMsgBatch final_batch;
      for (int t = 1; t <= 5; ++t) {
        program.receive({(int64_t)t, NumberData{t * 3.0}}, "i1");
        final_batch = program.receive({(int64_t)t, NumberData{1.0}}, "i2");
      }
      // No output yet (key stable)
      REQUIRE(final_batch.empty());

      // Key change triggers emission
      program.receive({6, NumberData{18.0}}, "i1");
      final_batch = program.receive({6, NumberData{2.0}}, "i2");

      THEN("Pipeline inside prototype processes correctly") {
        REQUIRE(final_batch.size() == 1);
        REQUIRE(final_batch["output1"]["o1"].size() == 1);

        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 6);  // boundary timestamp
        // Standard deviation of [6.0, 9.0, 12.0] is 3.0
        REQUIRE(out_msg->data.value == Approx(3.0));
      }
    }

    WHEN("Serializing and deserializing") {
      // Build up state: 5 messages with key=1, then serialize
      for (int t = 1; t <= 5; ++t) {
        program.receive({(int64_t)t, NumberData{t * 3.0}}, "i1");
        program.receive({(int64_t)t, NumberData{1.0}}, "i2");
      }

      auto serialized = program.serialize_data();
      Program restored(program_json);
      restored.restore_data_from_json(serialized);

      // Key change triggers emission on both
      program.receive({6, NumberData{18.0}}, "i1");
      auto orig_batch = program.receive({6, NumberData{2.0}}, "i2");

      restored.receive({6, NumberData{18.0}}, "i1");
      auto rest_batch = restored.receive({6, NumberData{2.0}}, "i2");

      THEN("State is preserved correctly") {
        REQUIRE(orig_batch.count("output1") == 1);
        REQUIRE(rest_batch.count("output1") == 1);

        const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_batch["output1"]["o1"].back().get());
        const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_batch["output1"]["o1"].back().get());
        REQUIRE(orig_msg != nullptr);
        REQUIRE(rest_msg != nullptr);
        REQUIRE(orig_msg->data.value == Approx(rest_msg->data.value));
      }
    }
  }
}

SCENARIO("Program handles nested Pipeline prototypes correctly", "[program][prototypes][pipeline]") {
  GIVEN("A program with a prototype containing a Pipeline with multiple operators") {
    // Tests prototype + Pipeline with control port.
    // Pipeline contains MA→Scale→STD chain. Input has 2 ports for data + control.
    // This validates that prototypes correctly expand Pipeline operators with
    // control connections through the prototype entry mechanism.
    std::string program_json = R"({
      "prototypes": {
        "processor": {
          "parameters": [
            {"name": "ma_window", "type": "number"},
            {"name": "scale_value", "type": "number", "default": 2.0},
            {"name": "std_window", "type": "number", "default": 3}
          ],
          "operators": [
            {
              "type": "Pipeline",
              "id": "pipe",
              "input_port_types": ["number"],
              "output_port_types": ["number"],
              "operators": [
                {"type": "MovingAverage", "id": "ma", "window_size": "${ma_window}"},
                {"type": "Scale", "id": "scale", "value": "${scale_value}"},
                {"type": "StandardDeviation", "id": "std", "window_size": "${std_window}"}
              ],
              "connections": [
                {"from": "ma", "to": "scale", "fromPort": "o1", "toPort": "i1"},
                {"from": "scale", "to": "std", "fromPort": "o1", "toPort": "i1"}
              ],
              "entryOperator": "ma",
              "outputMappings": {
                "std": {"o1": "o1"}
              }
            }
          ],
          "connections": [],
          "entry": {"operator": "pipe"},
          "output": {"operator": "pipe"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
        {
          "id": "proc1",
          "prototype": "processor",
          "parameters": {
            "ma_window": 2,
            "scale_value": 2.0,
            "std_window": 3
          }
        },
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "proc1", "fromPort": "o1", "toPort": "i1"},
        {"from": "input1", "to": "proc1", "fromPort": "o2", "toPort": "i1", "toPortType": "control"},
        {"from": "proc1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program program(program_json);

    WHEN("Processing messages") {
      // Pipeline: MA(2) → Scale(2.0) → STD(3)
      // MA(2) needs 2 values, Scale passes through (×2), STD(3) needs 3 values.
      //
      // t=1: MA collecting, key=1
      // t=2: MA emits avg(10,20)=15 → Scale emits 30 → STD collecting [30], key=1
      // t=3: MA emits avg(20,30)=25 → Scale emits 50 → STD collecting [30,50], key=1
      // t=4: MA emits avg(30,40)=35 → Scale emits 70 → STD emits std(30,50,70) → buffered, key=1
      // t=5: key change to 2 → emit buffer
      ProgramMsgBatch final_batch;
      for (int t = 1; t <= 4; ++t) {
        program.receive({(int64_t)t, NumberData{t * 10.0}}, "i1");
        final_batch = program.receive({(int64_t)t, NumberData{1.0}}, "i2");
      }
      // No output yet (key stable)
      REQUIRE(final_batch.empty());

      // Key change triggers emission
      program.receive({5, NumberData{50.0}}, "i1");
      final_batch = program.receive({5, NumberData{2.0}}, "i2");

      THEN("STD produces expected output for first set of values") {
        REQUIRE(final_batch.count("output1") == 1);
        REQUIRE(final_batch["output1"].count("o1") == 1);

        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 5);  // boundary timestamp

        // STD of [30, 50, 70]
        // mean = 50
        // variance = ((30-50)^2 + (50-50)^2 + (70-50)^2) / (3-1) = (400+0+400)/2 = 400
        // std = sqrt(400) = 20
        REQUIRE(out_msg->data.value == Approx(20.0));
      }
    }

    WHEN("Serializing and deserializing") {
      // Build up state: 4 messages with key=1
      for (int t = 1; t <= 4; ++t) {
        program.receive({(int64_t)t, NumberData{t * 10.0}}, "i1");
        program.receive({(int64_t)t, NumberData{1.0}}, "i2");
      }

      auto serialized = program.serialize_data();
      Program restored(program_json);
      restored.restore_data_from_json(serialized);

      // Key change triggers emission on both
      program.receive({5, NumberData{50.0}}, "i1");
      auto orig_batch = program.receive({5, NumberData{2.0}}, "i2");

      restored.receive({5, NumberData{50.0}}, "i1");
      auto rest_batch = restored.receive({5, NumberData{2.0}}, "i2");

      THEN("Both programs produce identical first output") {
        REQUIRE(orig_batch.count("output1") == 1);
        REQUIRE(rest_batch.count("output1") == 1);
        REQUIRE(orig_batch["output1"].count("o1") == 1);
        REQUIRE(rest_batch["output1"].count("o1") == 1);

        const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_batch["output1"]["o1"].back().get());
        const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_batch["output1"]["o1"].back().get());

        REQUIRE(orig_msg != nullptr);
        REQUIRE(rest_msg != nullptr);
        REQUIRE(orig_msg->time == 5);
        REQUIRE(rest_msg->time == 5);
        REQUIRE(orig_msg->data.value == Approx(rest_msg->data.value));
        REQUIRE(orig_msg->data.value == Approx(20.0));
      }
    }
  }

  GIVEN("A program with invalid nested pipeline parameters") {
    std::string invalid_json = R"({
      "prototypes": {
        "nestedProcessor": {
          "parameters": [
            {"name": "outer_window", "type": "number"}
          ],
          "operators": [
            {
              "type": "Pipeline",
              "id": "outer",
              "input_port_types": ["number"],
              "output_port_types": ["number"],
              "operators": [
                {
                  "type": "Pipeline",
                  "id": "inner",
                  "input_port_types": ["number"],
                  "output_port_types": ["number"],
                  "operators": [
                    {"type": "MovingAverage", "id": "ma", "window_size": "${nonexistent_param}"}
                  ],
                  "connections": [],
                  "entryOperator": "ma",
                  "outputMappings": {
                    "ma": {"o1": "o1"}
                  }
                }
              ],
              "connections": [],
              "entryOperator": "inner",
              "outputMappings": {
                "inner": {"o1": "o1"}
              }
            }
          ],
          "connections": [],
          "entry": {"operator": "outer"},
          "output": {"operator": "outer"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {
          "id": "proc1",
          "prototype": "nestedProcessor",
          "parameters": {
            "outer_window": 3
          }
        },
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "proc1"},
        {"from": "proc1", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    THEN("Program creation fails with appropriate error") {
      REQUIRE_THROWS_WITH(Program(invalid_json), Catch::Contains("Unknown parameter reference '${nonexistent_param}'"));
    }
  }
}

SCENARIO("Program with nested prototypes", "[program]") {
  std::string program_json = R"({
      "prototypes": {
        "simpleMA": {
          "parameters": [
            {"name": "window", "type": "number"}
          ],
          "operators": [
            {"type": "MovingAverage", "id": "ma", "window_size": "${window}"}
          ],
          "connections": [],
          "entry": {"operator": "ma"},
          "output": {"operator": "ma"}
        },
        "dualMA": {
          "parameters": [
            {"name": "window1", "type": "number"},
            {"name": "window2", "type": "number"}
          ],
          "operators": [
            {"id": "ma1", "prototype": "simpleMA", "parameters": {"window": "${window1}"}},
            {"id": "ma2", "prototype": "simpleMA", "parameters": {"window": "${window2}"}}
          ],
          "connections": [
            {"from": "ma1::ma", "to": "ma2::ma"}
          ],
          "entry": {"operator": "ma1::ma"},
          "output": {"operator": "ma2::ma"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"id": "nested_ma", "prototype": "dualMA", "parameters": {"window1": 2, "window2": 3}},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "nested_ma::ma1::ma"},
        {"from": "nested_ma::ma2::ma", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

  Program program(program_json);
  WHEN("Processing messages through nested moving averages") {
    ProgramMsgBatch batch;
    std::vector<double> results;

    // Send messages one by one
    batch = program.receive(Message<NumberData>(1, NumberData{2.0}));
    REQUIRE(batch.empty());  // First MA collecting

    batch = program.receive(Message<NumberData>(2, NumberData{4.0}));
    REQUIRE(batch.empty());  // First MA starts outputting, second MA collecting

    batch = program.receive(Message<NumberData>(3, NumberData{6.0}));
    REQUIRE(batch.empty());  // Second MA still collecting

    batch = program.receive(Message<NumberData>(4, NumberData{8.0}));
    REQUIRE(batch.size() == 1);
    REQUIRE(batch["output1"]["o1"].size() == 1);
    auto* msg = dynamic_cast<const Message<NumberData>*>(batch["output1"]["o1"].back().get());
    REQUIRE(msg->data.value == Approx(5.0));  // First output from nested MAs

    batch = program.receive(Message<NumberData>(5, NumberData{10.0}));
    REQUIRE(batch.size() == 1);
    msg = dynamic_cast<const Message<NumberData>*>(batch["output1"]["o1"].back().get());
    REQUIRE(msg->data.value == Approx(7.0));
  }
}