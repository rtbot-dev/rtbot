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
      "outputs": [
        {"operatorId": "output1", "ports": ["o1"]}
      ]
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
      "outputs": [
        {"operatorId": "output1", "ports": ["o1"]}
      ]
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
      "outputs": [
        {"operatorId": "output1", "ports": ["o1"]}
      ]
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
      "outputs": [
        {"operatorId": "output1", "ports": ["o1"]}
      ]
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
      "outputs": [
        {"operatorId": "output1", "ports": ["o1"]}
      ]
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