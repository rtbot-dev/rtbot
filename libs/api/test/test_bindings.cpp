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

      auto state = serialize_program_data("test_prog2");
      delete_program("test_prog2");
      create_program("test_prog2", program_json);
      restore_program_data_from_json("test_prog2", state);

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

SCENARIO("Bindings support staged vector messages", "[bindings][vector]") {
  auto& manager = ProgramManager::instance();
  manager.clear_all_programs();

  std::string vector_program_json = R"({
    "operators": [
      {"type": "Input", "id": "input1", "portTypes": ["vector_number"]},
      {"type": "Output", "id": "output1", "portTypes": ["vector_number"]}
    ],
    "connections": [
      {"from": "input1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
    ],
    "entryOperator": "input1",
    "output": { "output1": ["o1"] }
  })";

  REQUIRE(create_program("vector_prog_bindings", vector_program_json).empty());
  REQUIRE(push_vector_message_value("vector_prog_bindings", "i1", 1.0) == "0");
  REQUIRE(begin_vector_message("vector_prog_bindings", "i1", 10) == "1");
  REQUIRE(push_vector_message_value("vector_prog_bindings", "i1", 11.5) == "1");
  REQUIRE(push_vector_message_value("vector_prog_bindings", "i1", 22.5) == "1");
  REQUIRE(end_vector_message("vector_prog_bindings", "i1") == "1");

  auto result = json::parse(process_message_buffer("vector_prog_bindings"));
  REQUIRE(result.contains("output1"));
  REQUIRE(result["output1"].contains("o1"));
  REQUIRE(result["output1"]["o1"].size() == 1);
  REQUIRE(result["output1"]["o1"][0]["time"] == 10);
  REQUIRE(result["output1"]["o1"][0]["value"].is_array());
  REQUIRE(result["output1"]["o1"][0]["value"].size() == 2);
  REQUIRE(result["output1"]["o1"][0]["value"][0] == Approx(11.5));
  REQUIRE(result["output1"]["o1"][0]["value"][1] == Approx(22.5));
}

SCENARIO("Bindings accept inline KeyedPipeline prototypes", "[bindings][prototype]") {
  auto& manager = ProgramManager::instance();
  manager.clear_all_programs();

  std::string inline_keyed_program_json = R"({
    "operators": [
      {"type": "Input", "id": "input1", "portTypes": ["vector_number"]},
      {
        "type": "KeyedPipeline",
        "id": "keyed1",
        "prototype": {
          "operators": [
            {"type": "Input", "id": "proto_in", "portTypes": ["vector_number"]},
            {"type": "Output", "id": "proto_out", "portTypes": ["vector_number"]}
          ],
          "connections": [
            {"from": "proto_in", "to": "proto_out", "fromPort": "o1", "toPort": "i1"}
          ],
          "entry": {"operator": "proto_in"},
          "output": {"operator": "proto_out"}
        }
      },
      {"type": "Output", "id": "output1", "portTypes": ["vector_number"]}
    ],
    "connections": [
      {"from": "input1", "to": "keyed1", "fromPort": "o1", "toPort": "i1"},
      {"from": "keyed1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
    ],
    "entryOperator": "input1",
    "output": { "output1": ["o1"] }
  })";

  const auto create_result = create_program("inline_keyed_prog_bindings", inline_keyed_program_json);
  INFO("inline keyed create result: " << create_result);
  REQUIRE(create_result.empty());
  REQUIRE(begin_vector_message("inline_keyed_prog_bindings", "i1", 123) == "1");
  REQUIRE(push_vector_message_value("inline_keyed_prog_bindings", "i1", 7001.0) == "1");
  REQUIRE(push_vector_message_value("inline_keyed_prog_bindings", "i1", 42.5) == "1");
  REQUIRE(end_vector_message("inline_keyed_prog_bindings", "i1") == "1");

  auto result = json::parse(process_message_buffer("inline_keyed_prog_bindings"));
  REQUIRE(result.contains("output1"));
  REQUIRE(result["output1"].contains("o1"));
  REQUIRE(result["output1"]["o1"].size() == 1);
  REQUIRE(result["output1"]["o1"][0]["time"] == 123);
  REQUIRE(result["output1"]["o1"][0]["value"].is_array());
  // Key comes from msg->id (default 0), output is the sub-graph output without key prepended
  REQUIRE(result["output1"]["o1"][0]["value"].size() == 2);
  REQUIRE(result["output1"]["o1"][0]["value"][0] == Approx(7001.0));
  REQUIRE(result["output1"]["o1"][0]["value"][1] == Approx(42.5));
}

SCENARIO("Bindings support message id for keyed routing", "[bindings][id]") {
  auto& manager = ProgramManager::instance();
  manager.clear_all_programs();

  // KeyedPipeline with CumulativeSum sub-graph — state accumulates per key
  // VectorExtract(0) → CumulativeSum gives us a stateful sub-graph where
  // we can verify that messages with the same id share state.
  std::string keyed_cumsum_json = R"({
    "operators": [
      {"type": "Input", "id": "input1", "portTypes": ["vector_number"]},
      {
        "type": "KeyedPipeline",
        "id": "keyed1",
        "prototype": {
          "operators": [
            {"type": "VectorExtract", "id": "ext", "index": 0},
            {"type": "CumulativeSum", "id": "sum"}
          ],
          "connections": [
            {"from": "ext", "to": "sum", "fromPort": "o1", "toPort": "i1"}
          ],
          "entry": {"operator": "ext"},
          "output": {"operator": "sum"}
        }
      },
      {"type": "Output", "id": "output1", "portTypes": ["vector_number"]}
    ],
    "connections": [
      {"from": "input1", "to": "keyed1", "fromPort": "o1", "toPort": "i1"},
      {"from": "keyed1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
    ],
    "entryOperator": "input1",
    "output": { "output1": ["o1"] }
  })";

  SECTION("Messages with different ids accumulate independently") {
    REQUIRE(create_program("id_keyed_test", keyed_cumsum_json).empty());

    // id=1: [100.0] → cumsum=100
    REQUIRE(begin_vector_message("id_keyed_test", "i1", 1, 1) == "1");
    REQUIRE(push_vector_message_value("id_keyed_test", "i1", 100.0) == "1");
    REQUIRE(end_vector_message("id_keyed_test", "i1") == "1");
    auto r1 = json::parse(process_message_buffer("id_keyed_test"));
    REQUIRE(r1["output1"]["o1"][0]["value"][0] == Approx(100.0));

    // id=2: [200.0] → cumsum=200 (separate key, fresh sub-graph)
    REQUIRE(begin_vector_message("id_keyed_test", "i1", 2, 2) == "1");
    REQUIRE(push_vector_message_value("id_keyed_test", "i1", 200.0) == "1");
    REQUIRE(end_vector_message("id_keyed_test", "i1") == "1");
    auto r2 = json::parse(process_message_buffer("id_keyed_test"));
    REQUIRE(r2["output1"]["o1"][0]["value"][0] == Approx(200.0));

    // id=1 again: [50.0] → cumsum=150 (accumulates with first id=1 message)
    REQUIRE(begin_vector_message("id_keyed_test", "i1", 3, 1) == "1");
    REQUIRE(push_vector_message_value("id_keyed_test", "i1", 50.0) == "1");
    REQUIRE(end_vector_message("id_keyed_test", "i1") == "1");
    auto r3 = json::parse(process_message_buffer("id_keyed_test"));
    REQUIRE(r3["output1"]["o1"][0]["value"][0] == Approx(150.0));  // 100 + 50

    // id=2 again: [300.0] → cumsum=500 (accumulates with first id=2 message)
    REQUIRE(begin_vector_message("id_keyed_test", "i1", 4, 2) == "1");
    REQUIRE(push_vector_message_value("id_keyed_test", "i1", 300.0) == "1");
    REQUIRE(end_vector_message("id_keyed_test", "i1") == "1");
    auto r4 = json::parse(process_message_buffer("id_keyed_test"));
    REQUIRE(r4["output1"]["o1"][0]["value"][0] == Approx(500.0));  // 200 + 300
  }

  SECTION("Scalar add_to_message_buffer with explicit id=0") {
    std::string simple_json = R"({
      "operators": [
        {"type": "Input", "id": "in1", "portTypes": ["number"]},
        {"type": "Output", "id": "out1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "in1", "to": "out1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "in1",
      "output": { "out1": ["o1"] }
    })";
    REQUIRE(create_program("id_scalar_test", simple_json).empty());
    REQUIRE(add_to_message_buffer("id_scalar_test", "i1", 1, 99.0, 0) == "1");
    auto result = json::parse(process_message_buffer("id_scalar_test"));
    REQUIRE(result["out1"]["o1"][0]["time"] == 1);
    REQUIRE(result["out1"]["o1"][0]["value"] == Approx(99.0));
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
          // std::cout << pretty_print(batch) << std::endl;
          auto result = json::parse(batch);
          REQUIRE(result.contains("output1"));
          REQUIRE(result["output1"].contains("o1"));
        }
      }
    }
  }
}

SCENARIO("maxSizePerPort is preserved through JSON serialization round-trip", "[bindings][maxSizePerPort]") {
  GIVEN("Operators constructed with a custom maxSizePerPort via JSON") {
    size_t custom_limit = 42;

    WHEN("A Join operator is deserialized with maxSizePerPort") {
      std::string json_str = R"({"type":"Join","id":"j1","portTypes":["number","number"],"maxSizePerPort":42})";
      auto op = OperatorJson::read_op(json_str);
      THEN("max_size_per_port() returns the custom value") {
        REQUIRE(op->max_size_per_port() == custom_limit);
      }
      AND_THEN("write_op round-trips the value") {
        auto out = json::parse(OperatorJson::write_op(op));
        REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
      }
    }

    WHEN("An Addition operator is deserialized with maxSizePerPort") {
      std::string json_str = R"({"type":"Addition","id":"a1","numPorts":2,"maxSizePerPort":42})";
      auto op = OperatorJson::read_op(json_str);
      THEN("max_size_per_port() returns the custom value") {
        REQUIRE(op->max_size_per_port() == custom_limit);
      }
      AND_THEN("write_op round-trips the value") {
        auto out = json::parse(OperatorJson::write_op(op));
        REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
      }
    }

    WHEN("A Demultiplexer operator is deserialized with maxSizePerPort") {
      std::string json_str = R"({"type":"Demultiplexer","id":"d1","numPorts":2,"portType":"number","maxSizePerPort":42})";
      auto op = OperatorJson::read_op(json_str);
      THEN("max_size_per_port() returns the custom value") {
        REQUIRE(op->max_size_per_port() == custom_limit);
      }
      AND_THEN("write_op round-trips the value") {
        auto out = json::parse(OperatorJson::write_op(op));
        REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
      }
    }

    WHEN("A Variable operator is deserialized with maxSizePerPort") {
      std::string json_str = R"({"type":"Variable","id":"v1","default_value":0.0,"maxSizePerPort":42})";
      auto op = OperatorJson::read_op(json_str);
      THEN("max_size_per_port() returns the custom value") {
        REQUIRE(op->max_size_per_port() == custom_limit);
      }
      AND_THEN("write_op round-trips the value") {
        auto out = json::parse(OperatorJson::write_op(op));
        REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
      }
    }

    WHEN("A Subtraction operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"Subtraction","id":"s1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A Multiplication operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"Multiplication","id":"m1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A Division operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"Division","id":"d1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A Linear operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"Linear","id":"l1","coefficients":[1.0,2.0],"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A LogicalAnd operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"LogicalAnd","id":"la1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A LogicalOr operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"LogicalOr","id":"lo1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A LogicalXor operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"LogicalXor","id":"lx1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A LogicalNand operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"LogicalNand","id":"ln1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A LogicalNor operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"LogicalNor","id":"lno1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A LogicalImplication operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"LogicalImplication","id":"li1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A SyncGreaterThan operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"SyncGreaterThan","id":"sgt1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A SyncLessThan operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"SyncLessThan","id":"slt1","numPorts":2,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A SyncEqual operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"SyncEqual","id":"se1","numPorts":2,"epsilon":1e-10,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A SyncNotEqual operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"SyncNotEqual","id":"sne1","numPorts":2,"epsilon":1e-10,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A CompareSyncGT operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"CompareSyncGT","id":"csgt1","maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A CompareSyncLT operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"CompareSyncLT","id":"cslt1","maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A CompareSyncGTE operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"CompareSyncGTE","id":"csgte1","maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A CompareSyncLTE operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"CompareSyncLTE","id":"cslte1","maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A CompareSyncEQ operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"CompareSyncEQ","id":"cseq1","tolerance":0.0,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("A CompareSyncNEQ operator is deserialized with maxSizePerPort") {
      auto op = OperatorJson::read_op(R"({"type":"CompareSyncNEQ","id":"csneq1","tolerance":0.0,"maxSizePerPort":42})");
      REQUIRE(op->max_size_per_port() == custom_limit);
      auto out = json::parse(OperatorJson::write_op(op));
      REQUIRE(out["maxSizePerPort"].get<size_t>() == custom_limit);
    }

    WHEN("maxSizePerPort is omitted, the default is used") {
      std::string json_str = R"({"type":"Addition","id":"a1","numPorts":2})";
      auto op = OperatorJson::read_op(json_str);
      THEN("max_size_per_port() returns MAX_SIZE_PER_PORT") {
        REQUIRE(op->max_size_per_port() == MAX_SIZE_PER_PORT);
      }
    }
  }
}

SCENARIO("BooleanToNumber JSON round-trip", "[bindings][BooleanToNumber]") {
  GIVEN("A BooleanToNumber operator created via read_op") {
    std::string json_str = R"({"type":"BooleanToNumber","id":"b2n1"})";
    auto op = OperatorJson::read_op(json_str);

    THEN("The operator has the correct type and id") {
      REQUIRE(op->type_name() == "BooleanToNumber");
      REQUIRE(op->id() == "b2n1");
    }

    WHEN("Serialized via write_op") {
      auto out = json::parse(OperatorJson::write_op(op));

      THEN("Round-trip preserves type and id") {
        REQUIRE(out["type"].get<std::string>() == "BooleanToNumber");
        REQUIRE(out["id"].get<std::string>() == "b2n1");
      }

      AND_THEN("Re-reading the serialized JSON produces an equivalent operator") {
        auto op2 = OperatorJson::read_op(out.dump());
        REQUIRE(op2->type_name() == "BooleanToNumber");
        REQUIRE(op2->id() == "b2n1");
      }
    }
  }
}
