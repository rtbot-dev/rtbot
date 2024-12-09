#include <iostream>
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "rtbot/Program.h"

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
      REQUIRE(manager.add_to_message_buffer("prog1", "input1", msg));

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
        REQUIRE_FALSE(manager.add_to_message_buffer("prog1", "input1", Message<NumberData>(1, NumberData{42.0})));
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
      std::vector<Message<NumberData>> messages = {{1, NumberData{3.0}}, {2, NumberData{6.0}}, {3, NumberData{9.0}}};

      ProgramMsgBatch final_batch;
      for (const auto& msg : messages) {
        final_batch = program.receive(msg);
      }

      THEN("Pipeline processes messages correctly") {
        REQUIRE(final_batch.size() == 1);
        REQUIRE(final_batch["output1"]["o1"].size() == 1);
        const auto* out_msg = dynamic_cast<const Message<NumberData>*>(final_batch["output1"]["o1"].back().get());
        REQUIRE(out_msg != nullptr);
        REQUIRE(out_msg->time == 3);
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
                        {"type": "MovingAverage", "id": "ma1", "window_size": 3}
                    ],
                    "connections": [],
                    "entryOperator": "ma1",
                    "outputMappings": {
                        "ma1": {"o1": "o1"}
                    }
                },
                {"type": "Output", "id": "output1", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input1", "to": "pipeline1"},
                {"from": "pipeline1", "to": "output1"}
            ],
            "entryOperator": "input1",
            "output": {
                "output1": ["o1"]
            }
        })";

    THEN("Program creation fails with appropriate error") {
      REQUIRE_THROWS_WITH(Program(invalid_program), Catch::Contains("Pipeline must contain at least two operators"));
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
                        {"type": "StandardDeviation", "id": "std1", "window_size": 3},
                        {"type": "MovingAverage", "id": "ma2", "window_size": 2}
                    ],
                    "connections": [
                        {"from": "ma1", "to": "std1", "fromPort": "o1", "toPort": "i1"},
                        {"from": "std1", "to": "ma2", "fromPort": "o1", "toPort": "i1"}
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

    WHEN("Processing messages and serializing") {
      // Feed more initial data points to establish state
      std::vector<std::pair<int64_t, double>> initial_data = {{1, 3.0},  {2, 6.0},  {3, 9.0},  {4, 12.0},
                                                              {5, 15.0}, {6, 18.0}, {7, 21.0}, {8, 24.0}};

      for (const auto& [time, value] : initial_data) {
        original_program.receive(Message<NumberData>(time, NumberData{value}));
      }

      // Serialize and restore
      Bytes serialized = original_program.serialize();
      Program restored_program(serialized);

      // Process new message on both programs
      auto original_batch = original_program.receive(Message<NumberData>(9, NumberData{27.0}));
      auto restored_batch = restored_program.receive(Message<NumberData>(9, NumberData{27.0}));

      THEN("Pipeline state is correctly preserved") {
        // Debug output for batches
        INFO("Original batch size: " << original_batch.size());
        INFO("Restored batch size: " << restored_batch.size());

        // Check batch structure
        REQUIRE(original_batch.size() == restored_batch.size());

        // Debug output for operator content
        for (const auto& [op_id, op_batch] : original_batch) {
          INFO("Original operator " << op_id << " contains ports:");
          for (const auto& [port_name, port_msgs] : op_batch) {
            INFO(" - Port " << port_name << " has " << port_msgs.size() << " messages");
            for (const auto& msg : port_msgs) {
              const auto* num_msg = dynamic_cast<const Message<NumberData>*>(msg.get());
              if (num_msg) {
                INFO("   Time: " << num_msg->time << ", Value: " << num_msg->data.value);
              }
            }
          }
        }

        for (const auto& [op_id, op_batch] : restored_batch) {
          INFO("Restored operator " << op_id << " contains ports:");
          for (const auto& [port_name, port_msgs] : op_batch) {
            INFO(" - Port " << port_name << " has " << port_msgs.size() << " messages");
            for (const auto& msg : port_msgs) {
              const auto* num_msg = dynamic_cast<const Message<NumberData>*>(msg.get());
              if (num_msg) {
                INFO("   Time: " << num_msg->time << ", Value: " << num_msg->data.value);
              }
            }
          }
        }

        REQUIRE(original_batch.count("output1") == 1);
        REQUIRE(restored_batch.count("output1") == 1);
        REQUIRE(original_batch.at("output1").count("o1") == 1);
        REQUIRE(restored_batch.at("output1").count("o1") == 1);

        const auto& original_msgs = original_batch.at("output1").at("o1");
        const auto& restored_msgs = restored_batch.at("output1").at("o1");

        REQUIRE(original_msgs.size() == restored_msgs.size());

        for (size_t i = 0; i < original_msgs.size(); i++) {
          const auto* original_msg = dynamic_cast<const Message<NumberData>*>(original_msgs[i].get());
          const auto* restored_msg = dynamic_cast<const Message<NumberData>*>(restored_msgs[i].get());

          REQUIRE(original_msg != nullptr);
          REQUIRE(restored_msg != nullptr);

          INFO("Message " << i << " comparison:");
          INFO("Original - Time: " << original_msg->time << ", Value: " << original_msg->data.value);
          INFO("Restored - Time: " << restored_msg->time << ", Value: " << restored_msg->data.value);

          REQUIRE(original_msg->time == restored_msg->time);
          REQUIRE(original_msg->data.value == Approx(restored_msg->data.value));
        }
      }
    }
  }
}
