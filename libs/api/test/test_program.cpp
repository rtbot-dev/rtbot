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
      Bytes serialized = original_program.serialize();
      Program restored_program(serialized);

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
  GIVEN("A sprogram with a Pipeline containing multiple connected operators") {
    std::string program_json = R"({
            "operators": [
                {"type": "Input", "id": "input1", "portTypes": ["number"]},
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
                {"from": "pipeline1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input1",
            "output": {
                "output1": ["o1"]
            }
        })";

    Program program(program_json);

    WHEN("Processing messages") {
      // Need 5 messages:
      // First 3 messages to fill MA(3) buffer
      // Message 4 and 5 to emit MA values that will fill STD(3) buffer and produce output
      std::vector<Message<NumberData>> messages = {
          {1, NumberData{3.0}},   // MA collecting
          {2, NumberData{6.0}},   // MA collecting
          {3, NumberData{9.0}},   // MA emits first value (6.0) -> STD collecting
          {4, NumberData{12.0}},  // MA emits second value (9.0) -> STD collecting
          {5, NumberData{15.0}}   // MA emits third value (12.0) -> STD emits first value
      };

      ProgramMsgBatch final_batch;
      for (const auto& msg : messages) {
        final_batch = program.receive(msg);
      }

      THEN("Pipeline processes messages correctly") {
        REQUIRE(final_batch.size() == 1);
        REQUIRE(final_batch["output1"].count("o1") == 1);
        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 5);
        // Don't check exact value as StdDev output depends on MovingAverage behavior
      }
    }

    WHEN("Processing fewer messages") {
      // Only send 4 messages - not enough for complete pipeline output
      std::vector<Message<NumberData>> messages = {
          {1, NumberData{3.0}},  // MA collecting
          {2, NumberData{6.0}},  // MA collecting
          {3, NumberData{9.0}},  // MA emits first value (6.0) -> STD collecting
          {4, NumberData{12.0}}  // MA emits second value (9.0) -> STD collecting (needs one more)
      };

      ProgramMsgBatch final_batch;
      for (const auto& msg : messages) {
        final_batch = program.receive(msg);
      }

      THEN("No output is produced yet") {
        REQUIRE(final_batch.empty());  // No output until STD has enough values
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

SCENARIO("Program handles Pipeline serialization", "[program][pipeline]") {
  GIVEN("A program with a stateful Pipeline") {
    std::string program_json = R"({
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
                {"from": "pipeline1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
            ],
            "entryOperator": "input1",
            "output": {
                "output1": ["o1"]
            }
        })";

    Program original_program(program_json);

    WHEN("Processing messages and serializing at critical state") {
      // First 3 messages to get MA1's first output to MA2
      std::vector<std::pair<int64_t, double>> initial_sequence = {
          {1, 10.0},  // MA1: collecting
          {2, 20.0},  // MA1: collecting
          {3, 30.0}   // MA1: first output (20) -> MA2: collecting
                      // Critical state: MA2 has its first value but hasn't emitted yet
      };

      // Process first 3 messages
      ProgramMsgBatch batch;
      for (const auto& [time, value] : initial_sequence) {
        batch = original_program.receive(Message<NumberData>(time, NumberData{value}));
      }

      // Verify no output yet (need 4th message for that)
      REQUIRE(batch.empty());

      // Serialize at critical state (MA2 has first value but hasn't emitted)
      Bytes serialized = original_program.serialize();
      Program restored_program(serialized);

      // Send 4th message to both programs
      auto original_batch = original_program.receive(Message<NumberData>(4, NumberData{40.0}));
      auto restored_batch = restored_program.receive(Message<NumberData>(4, NumberData{40.0}));

      THEN("Both programs produce identical first output") {
        // Verify both programs produce output
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
        REQUIRE(original_msg->time == 4);
        REQUIRE(restored_msg->time == 4);

        // Both should output 25.0 (average of MA1's first two outputs: 20,30)
        REQUIRE(original_msg->data.value == Approx(25.0));
        REQUIRE(restored_msg->data.value == Approx(25.0));

        // After this output, both programs should have reset their state
        // Let's verify this by sending another message
        auto original_next = original_program.receive(Message<NumberData>(5, NumberData{50.0}));
        auto restored_next = restored_program.receive(Message<NumberData>(5, NumberData{50.0}));

        // Both should produce no output as they're in first message after reset
        REQUIRE(original_next.empty());
        REQUIRE(restored_next.empty());
      }
    }
  }
}

SCENARIO("Pipeline reset and emission behavior", "[program][pipeline]") {
  GIVEN("A pipeline that requires state reset after emission") {
    // Create a program with a pipeline containing stateful operators
    std::string program_json = R"({
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
        {"from": "pipeline1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program original_program(program_json);

    WHEN("Processing messages before and after serialization") {
      // First sequence to get initial output (4 messages needed)
      std::vector<std::pair<int64_t, double>> sequence1 = {
          {1, 10.0},  // MA1: collecting
          {2, 20.0},  // MA1: collecting
          {3, 30.0},  // MA1: first output (20)
          {4, 40.0}   // MA1: second output (30) -> MA2: first output (25)
                      // Pipeline produces output and RESETS
      };

      ProgramMsgBatch last_batch;
      for (const auto& [time, value] : sequence1) {
        last_batch = original_program.receive(Message<NumberData>(time, NumberData{value}));
      }

      // Verify first sequence produced output
      REQUIRE(last_batch.count("output1") == 1);
      REQUIRE(last_batch["output1"].count("o1") == 1);

      const auto* first_output = dynamic_cast<const Message<NumberData>*>(last_batch["output1"]["o1"].back().get());
      REQUIRE(first_output != nullptr);
      INFO("First output value: " << first_output->data.value);
      REQUIRE(first_output->data.value == Approx(25.0));  // Average of first two MA1 outputs (20,30)

      // Process 4 more messages to get second output after reset
      std::vector<std::pair<int64_t, double>> sequence2 = {
          {5, 50.0},  // MA1: collecting (fresh start after reset)
          {6, 60.0},  // MA1: collecting
          {7, 70.0},  // MA1: first output (60)
          {8, 80.0}   // MA1: second output (70) -> MA2: first output (65)
                      // Pipeline produces output and RESETS again
      };

      ProgramMsgBatch second_batch;
      for (const auto& [time, value] : sequence2) {
        second_batch = original_program.receive(Message<NumberData>(time, NumberData{value}));
      }

      // Verify second output
      REQUIRE(second_batch.count("output1") == 1);
      REQUIRE(second_batch["output1"].count("o1") == 1);

      const auto* second_output = dynamic_cast<const Message<NumberData>*>(second_batch["output1"]["o1"].back().get());
      REQUIRE(second_output != nullptr);
      INFO("Second output value: " << second_output->data.value);
      REQUIRE(second_output->data.value == Approx(65.0));  // Average of first two MA1 outputs after reset (60,70)

      // Now serialize and restore
      Bytes serialized = original_program.serialize();
      Program restored_program(serialized);

      // Process another sequence on both programs - need 4 messages for output
      std::vector<std::pair<int64_t, double>> sequence3 = {
          {9, 90.0},    // MA1: collecting (fresh start)
          {10, 100.0},  // MA1: collecting
          {11, 110.0},  // MA1: first output (100)
          {12, 120.0}   // MA1: second output (110) -> MA2: first output (105)
                        // Pipeline produces output and RESETS
      };

      ProgramMsgBatch original_batch, restored_batch;
      for (const auto& [time, value] : sequence3) {
        original_batch = original_program.receive(Message<NumberData>(time, NumberData{value}));
        restored_batch = restored_program.receive(Message<NumberData>(time, NumberData{value}));
      }

      THEN("Both programs maintain correct reset behavior") {
        // Verify both programs produce output
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

        // Both programs should output 105 (average of 100,110 after fresh reset)
        REQUIRE(original_msg->data.value == Approx(105.0));
        REQUIRE(restored_msg->data.value == Approx(105.0));
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

      Bytes serialized = program.serialize();
      Program restored(serialized);

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
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {
          "id": "proc1",
          "prototype": "complexProcessor",
          "parameters": {"ma_window": 3}
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

    Program program(program_json);

    WHEN("Processing messages") {
      // Need 5 messages minimum:
      // First 3 for MA to produce output
      // Then 2 more for STD to produce output based on MA values
      std::vector<Message<NumberData>> messages = {
          {1, NumberData{3.0}},   // MA collecting
          {2, NumberData{6.0}},   // MA collecting
          {3, NumberData{9.0}},   // MA emits first value (6.0) -> STD collecting
          {4, NumberData{12.0}},  // MA emits second value (9.0) -> STD collecting
          {5, NumberData{15.0}}   // MA emits third value (12.0) -> STD emits first value
      };

      ProgramMsgBatch final_batch;
      for (const auto& msg : messages) {
        final_batch = program.receive(msg);
      }

      THEN("Pipeline inside prototype processes correctly") {
        REQUIRE(final_batch.size() == 1);
        REQUIRE(final_batch["output1"]["o1"].size() == 1);

        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 5);
        // Standard deviation of [6.0, 9.0, 12.0] is 3.0
        REQUIRE(out_msg->data.value == Approx(3.0));
      }
    }

    WHEN("Serializing and deserializing") {
      for (int i = 1; i <= 4; i++) {
        program.receive(Message<NumberData>(i, NumberData{i * 3.0}));
      }

      Bytes serialized = program.serialize();
      Program restored(serialized);

      auto orig_batch = program.receive(Message<NumberData>(5, NumberData{15.0}));
      auto rest_batch = restored.receive(Message<NumberData>(5, NumberData{15.0}));

      THEN("State is preserved correctly") {
        const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_batch["output1"]["o1"].back().get());
        const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_batch["output1"]["o1"].back().get());
        REQUIRE(orig_msg->data.value == Approx(rest_msg->data.value));
      }
    }
  }
}

SCENARIO("Program handles nested Pipeline prototypes correctly", "[program][prototypes][pipeline]") {
  GIVEN("A program with a prototype containing nested Pipelines") {
    std::string program_json = R"({
      "prototypes": {
        "nestedProcessor": {
          "parameters": [
            {"name": "outer_window", "type": "number"},
            {"name": "inner_window", "type": "number"},
            {"name": "scale", "type": "number", "default": 1.0}
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
                    {"type": "MovingAverage", "id": "ma", "window_size": "${inner_window}"},
                    {"type": "Scale", "id": "scale", "value": "${scale}"}
                  ],
                  "connections": [
                    {"from": "ma", "to": "scale", "fromPort": "o1", "toPort": "i1"}
                  ],
                  "entryOperator": "ma",
                  "outputMappings": {
                    "scale": {"o1": "o1"}
                  }
                },
                {"type": "StandardDeviation", "id": "std", "window_size": "${outer_window}"}
              ],
              "connections": [
                {"from": "inner", "to": "std", "fromPort": "o1", "toPort": "i1"}
              ],
              "entryOperator": "inner",
              "outputMappings": {
                "std": {"o1": "o1"}
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
            "outer_window": 3,
            "inner_window": 2,
            "scale": 2.0
          }
        },
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "proc1", "fromPort": "o1", "toPort": "i1"},
        {"from": "proc1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {
        "output1": ["o1"]
      }
    })";

    Program program(program_json);

    WHEN("Processing messages") {
      std::vector<Message<NumberData>> messages = {
          {1, NumberData{10.0}},  // MA collecting
          {2, NumberData{20.0}},  // MA emits avg(10,20)=15 * scale(2) = 30 -> STD collecting
                                  // Inner pipeline resets
          {3, NumberData{30.0}},  // MA collecting (fresh start)
          {4, NumberData{40.0}},  // MA emits avg(30,40)=35 * scale(2) = 70 -> STD collecting
                                  // Inner pipeline resets
          {5, NumberData{50.0}},  // MA collecting (fresh start)
          {6, NumberData{60.0}},  // MA emits avg(50,60)=55 * scale(2) = 110 -> STD collecting + emits
                                  // STD now has [30,70,110] and emits
          {7, NumberData{70.0}},  // MA collecting (fresh start)
          {8, NumberData{80.0}},  // MA emits avg(70,80)=75 * scale(2) = 150 -> STD collecting
                                  // Inner pipeline resets
          {9, NumberData{90.0}}   // MA collecting (fresh start)
      };

      for (const auto& msg : messages) {
        auto final_batch = program.receive_debug(msg);
        if (msg.time == 6) {  // First STD output
          THEN("STD produces expected output for first set of values") {
            REQUIRE(final_batch.count("output1") == 1);
            REQUIRE(final_batch["output1"].count("o1") == 1);

            const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
            REQUIRE(out_msg != nullptr);
            REQUIRE(out_msg->time == 6);

            // STD of [30,70,110]
            // mean = 70
            double expected_std = std::sqrt(
                ((30.0 - 70.0) * (30.0 - 70.0) + (70.0 - 70.0) * (70.0 - 70.0) + (110.0 - 70.0) * (110.0 - 70.0)) /
                (3 - 1));
            REQUIRE(out_msg->data.value == Approx(expected_std));
          }
        } else {
          THEN("No output at time " + std::to_string(msg.time)) {
            auto it = final_batch.find("output1");
            if (it != final_batch.end()) {
              REQUIRE(it->second["o1"].empty());
            }
          }
        }
      }
    }

    WHEN("Serializing and deserializing") {
      // Process first 5 messages (not enough for STD output)
      for (int i = 1; i <= 5; i++) {
        program.receive(Message<NumberData>(i, NumberData{i * 10.0}));
      }

      Bytes serialized = program.serialize();
      Program restored(serialized);

      // Send message that will produce first STD output
      auto orig_batch = program.receive(Message<NumberData>(6, NumberData{60.0}));
      auto rest_batch = restored.receive(Message<NumberData>(6, NumberData{60.0}));

      THEN("Both programs produce identical first output") {
        REQUIRE(orig_batch.count("output1") == 1);
        REQUIRE(rest_batch.count("output1") == 1);
        REQUIRE(orig_batch["output1"].count("o1") == 1);
        REQUIRE(rest_batch["output1"].count("o1") == 1);

        const auto* orig_msg = dynamic_cast<const Message<NumberData>*>(orig_batch["output1"]["o1"].back().get());
        const auto* rest_msg = dynamic_cast<const Message<NumberData>*>(rest_batch["output1"]["o1"].back().get());

        REQUIRE(orig_msg != nullptr);
        REQUIRE(rest_msg != nullptr);
        REQUIRE(orig_msg->time == 6);
        REQUIRE(rest_msg->time == 6);
        REQUIRE(orig_msg->data.value == Approx(rest_msg->data.value));

        // Verify state reset by processing another message
        auto orig_next = program.receive(Message<NumberData>(7, NumberData{70.0}));
        auto rest_next = restored.receive(Message<NumberData>(7, NumberData{70.0}));

        // Both should have no output after reset
        REQUIRE(orig_next.empty());
        REQUIRE(rest_next.empty());
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