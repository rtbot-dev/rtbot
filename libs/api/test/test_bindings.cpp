#include <catch2/catch.hpp>

#include "rtbot/Program.h"
#include "rtbot/bindings.h"

using namespace rtbot;

SCENARIO("Bindings handle program creation and message processing", "[bindings]") {
  GIVEN("A simple program JSON") {
    std::string program_json = R"({
            "operators": [
                {"type": "Input", "id": "input1", "portTypes": ["number"]},
                {"type": "Output", "id": "output1", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "input1", "to": "output1"}
            ],
            "entryOperator": "input1",
            "output": { "output1": ["o1"] }
        })";

    WHEN("Validating program") {
      REQUIRE(create_program("test_prog_bindings", program_json).empty());
      auto result = json::parse(validate_program(program_json));
      THEN("Validation succeeds") { REQUIRE(result["valid"].get<bool>()); }
    }

    WHEN("Creating and processing messages") {
      REQUIRE(add_to_message_buffer("test_prog_bindings", "i1", 1, 42.0) == "1");

      auto result = json::parse(process_message_buffer("test_prog_bindings"));

      THEN("Messages are processed correctly") {
        REQUIRE(result.contains("output1"));
        REQUIRE(result["output1"].contains("o1"));
        REQUIRE(result["output1"]["o1"].size() == 1);
        REQUIRE(result["output1"]["o1"][0]["time"] == 1);
        REQUIRE(result["output1"]["o1"][0]["value"] == 42.0);
      }
    }
  }

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
            "output": { "output1": ["o1"] }
        })";

    WHEN("Testing serialization") {
      REQUIRE(create_program("test_prog2", program_json).empty());

      std::vector<uint64_t> times = {1, 2};
      std::vector<double> values = {3.0, 6.0};
      std::vector<std::string> ports = {"i1", "i1"};

      process_batch("test_prog2", times, values, ports);

      auto state = serialize_program("test_prog2");
      delete_program("test_prog2");
      create_program_from_bytes("test_prog2", state);

      THEN("State is preserved") {
        std::vector<uint64_t> new_times = {3};
        std::vector<double> new_values = {9.0};
        std::vector<std::string> new_ports = {"i1"};

        auto result = json::parse(process_batch("test_prog2", new_times, new_values, new_ports));

        REQUIRE(result["output1"]["o1"].size() == 1);
        REQUIRE(result["output1"]["o1"][0]["value"] == Approx(6.0));
      }
    }
  }

  GIVEN("Debug mode") {
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
            "output": { "output1": ["o1"] }
        })";

    WHEN("Processing in debug mode") {
      REQUIRE(create_program("test_prog3", program_json).empty());

      process_batch_debug("test_prog3", {1, 2, 3, 4}, {3.0, 6.0, 9.0, 12.0}, {"i1", "i1", "i1", "i1"});

      auto batch = process_batch_debug("test_prog3", {5}, {20.0}, {"i1"});

      auto result = json::parse(batch);

      THEN("All operator outputs are captured") {
        REQUIRE(result.contains("input1"));
        REQUIRE(result.contains("ma1"));
        REQUIRE(result.contains("output1"));
      }
    }
  }
}

SCENARIO("Program and operator validation", "[bindings]") {
  auto& manager = ProgramManager::instance();
  manager.clear_all_programs();

  GIVEN("Various program configurations") {
    // Valid program JSON
    std::string valid_program = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": { "output1": ["o1"] }
    })";

    // Invalid program - missing entry operator
    std::string invalid_program_missing_entry = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "output": { "output1": ["o1"] }
    })";

    // Invalid program - mismatched port types
    std::string invalid_program_type_mismatch = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["boolean"]}
      ],
      "connections": [
        {"from": "input1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": { "output1": ["o1"] }
    })";

    WHEN("Validating a valid program") {
      auto result = json::parse(validate_program(valid_program));

      THEN("Validation succeeds") {
        REQUIRE(result["valid"].get<bool>());
        REQUIRE_FALSE(result.contains("error"));
      }
    }

    WHEN("Validating a program missing entry operator") {
      auto result = json::parse(validate_program(invalid_program_missing_entry));

      THEN("Validation fails with appropriate error") {
        REQUIRE_FALSE(result["valid"].get<bool>());
        REQUIRE(result["error"].get<std::string>().find("entryOperator") != std::string::npos);
      }
    }

    WHEN("Validating a program with type mismatch") {
      auto result = json::parse(validate_program(invalid_program_type_mismatch));

      THEN("Program validation passes but creation fails") {
        REQUIRE(result["valid"].get<bool>());
        auto creation_result = create_program("test_prog_bad", invalid_program_type_mismatch);
        REQUIRE_FALSE(creation_result.empty());
        REQUIRE(creation_result.find("type mismatch") != std::string::npos);
      }
    }
  }

  GIVEN("Various operator configurations") {
    // Valid moving average operator
    std::string valid_ma = R"({
      "type": "MovingAverage",
      "id": "ma1",
      "window_size": 3
    })";

    // Invalid moving average - missing window size
    std::string invalid_ma = R"({
      "type": "MovingAverage",
      "id": "ma1"
    })";

    // Valid input operator
    std::string valid_input = R"({
      "type": "Input",
      "id": "input1",
      "portTypes": ["number"]
    })";

    // Invalid input - unknown port type
    std::string invalid_input = R"({
      "type": "Input",
      "id": "input1",
      "portTypes": ["unknown_type"]
    })";

    WHEN("Validating a valid MovingAverage operator") {
      auto result = json::parse(validate_operator("MovingAverage", valid_ma));

      THEN("Validation succeeds") {
        REQUIRE(result["valid"].get<bool>());
        REQUIRE_FALSE(result.contains("error"));
      }
    }

    WHEN("Validating an invalid MovingAverage operator") {
      auto result = json::parse(validate_operator("MovingAverage", invalid_ma));

      THEN("Validation fails with appropriate error") {
        REQUIRE_FALSE(result["valid"].get<bool>());
        REQUIRE(result["error"].get<std::string>().find("window_size") != std::string::npos);
      }
    }

    WHEN("Validating a valid Input operator") {
      auto result = json::parse(validate_operator("Input", valid_input));

      THEN("Validation succeeds") {
        REQUIRE(result["valid"].get<bool>());
        REQUIRE_FALSE(result.contains("error"));
      }
    }

    WHEN("Validating an invalid Input operator") {
      auto result = json::parse(validate_operator("Input", invalid_input));

      THEN("Validation fails with appropriate error") {
        REQUIRE_FALSE(result["valid"].get<bool>());
        REQUIRE(result["error"].get<std::string>().find("portTypes") != std::string::npos);
      }
    }

    WHEN("Validating an unknown operator type") {
      auto result = json::parse(validate_operator("UnknownType", "{}"));

      THEN("Validation fails with unknown type error") {
        REQUIRE_FALSE(result["valid"].get<bool>());
        REQUIRE(result["error"].get<std::string>().find("Unknown operator type") != std::string::npos);
      }
    }
  }

  GIVEN("Complex program configurations") {
    // Program with multiple connected operators
    std::string complex_program = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "MovingAverage", "id": "ma1", "window_size": 3},
        {"type": "StandardDeviation", "id": "std1", "window_size": 5},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
        {"from": "ma1", "to": "std1", "fromPort": "o1", "toPort": "i1"},
        {"from": "std1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": { "output1": ["o1"] }
    })";

    WHEN("Validating and creating a complex program") {
      auto validation_result = json::parse(validate_program(complex_program));

      THEN("Validation and creation succeed") {
        REQUIRE(validation_result["valid"].get<bool>());
        auto creation_result = create_program("complex_prog", complex_program);
        REQUIRE(creation_result.empty());

        AND_THEN("Program processes messages correctly") {
          std::vector<uint64_t> times = {1, 2, 3, 4, 5, 6, 7};
          std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
          std::vector<std::string> ports(7, "i1");

          auto batch = process_batch("complex_prog", times, values, ports);
          std::cout << pretty_print(batch) << std::endl;
          auto result = json::parse(batch);
          REQUIRE(result.contains("output1"));
          REQUIRE(result["output1"].contains("o1"));
        }
      }
    }
  }
}