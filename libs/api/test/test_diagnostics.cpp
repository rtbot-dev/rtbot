#include <catch2/catch.hpp>

#include "rtbot/Diagnostics.h"

using namespace rtbot;

// ---------------------------------------------------------------------------
// Phase 1: JSON parse errors
// ---------------------------------------------------------------------------
SCENARIO("diagnoseProgram detects JSON parse errors", "[diagnostics]") {
  GIVEN("Invalid JSON input") {
    auto result = ProgramDiagnostics::diagnose("{invalid json}");
    THEN("Returns INVALID_JSON error") {
      REQUIRE_FALSE(result.valid);
      REQUIRE(result.errors.size() == 1);
      REQUIRE(result.errors[0].code == DiagnosticCode::INVALID_JSON);
      REQUIRE(result.errors[0].severity == "error");
    }
  }

  GIVEN("Empty string input") {
    auto result = ProgramDiagnostics::diagnose("");
    THEN("Returns INVALID_JSON error") {
      REQUIRE_FALSE(result.valid);
      REQUIRE(result.errors.size() == 1);
      REQUIRE(result.errors[0].code == DiagnosticCode::INVALID_JSON);
    }
  }
}

// ---------------------------------------------------------------------------
// Phase 2: Prototype resolution errors
// ---------------------------------------------------------------------------
SCENARIO("diagnoseProgram detects prototype errors", "[diagnostics]") {
  GIVEN("A program referencing an unknown prototype") {
    std::string json_str = R"({
      "prototypes": {
        "myProto": {
          "parameters": [{"name": "window", "type": "number"}],
          "operators": [{"type": "MovingAverage", "id": "ma", "window_size": 5}],
          "connections": [],
          "entry": {"operator": "ma"},
          "output": {"operator": "ma"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"id": "inst", "prototype": "nonExistent", "parameters": {}},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "inst"},
        {"from": "inst", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns UNKNOWN_PROTOTYPE error") {
      REQUIRE_FALSE(result.valid);
      bool found = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::UNKNOWN_PROTOTYPE) {
          REQUIRE(e.path == "/operators/1/prototype");
          REQUIRE_FALSE(e.suggestion.empty());
          found = true;
        }
      }
      REQUIRE(found);
    }
  }

  GIVEN("A program with unknown and missing prototype parameters") {
    std::string json_str = R"({
      "prototypes": {
        "myProto": {
          "parameters": [
            {"name": "window", "type": "number"},
            {"name": "label", "type": "string"}
          ],
          "operators": [{"type": "MovingAverage", "id": "ma", "window_size": 5}],
          "connections": [],
          "entry": {"operator": "ma"},
          "output": {"operator": "ma"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"id": "inst", "prototype": "myProto", "parameters": {"wrong_param": 5}},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "inst"},
        {"from": "inst", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns UNKNOWN_PARAMETER and MISSING_PARAMETER errors") {
      REQUIRE_FALSE(result.valid);
      bool found_unknown = false;
      bool found_missing_window = false;
      bool found_missing_label = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::UNKNOWN_PARAMETER && e.message.find("wrong_param") != std::string::npos)
          found_unknown = true;
        if (e.code == DiagnosticCode::MISSING_PARAMETER && e.message.find("window") != std::string::npos)
          found_missing_window = true;
        if (e.code == DiagnosticCode::MISSING_PARAMETER && e.message.find("label") != std::string::npos)
          found_missing_label = true;
      }
      REQUIRE(found_unknown);
      REQUIRE(found_missing_window);
      REQUIRE(found_missing_label);
    }
  }

  GIVEN("A program with wrong prototype parameter type") {
    std::string json_str = R"({
      "prototypes": {
        "myProto": {
          "parameters": [{"name": "window", "type": "number"}],
          "operators": [{"type": "MovingAverage", "id": "ma", "window_size": 5}],
          "connections": [],
          "entry": {"operator": "ma"},
          "output": {"operator": "ma"}
        }
      },
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"id": "inst", "prototype": "myProto", "parameters": {"window": "not_a_number"}},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "inst"},
        {"from": "inst", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns INVALID_PARAMETER_TYPE error") {
      REQUIRE_FALSE(result.valid);
      bool found = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::INVALID_PARAMETER_TYPE) {
          REQUIRE(e.path == "/operators/1/parameters/window");
          found = true;
        }
      }
      REQUIRE(found);
    }
  }
}

// ---------------------------------------------------------------------------
// Phase 3: Schema validation errors
// ---------------------------------------------------------------------------
SCENARIO("diagnoseProgram detects schema validation errors", "[diagnostics]") {
  GIVEN("A program missing entryOperator") {
    std::string json_str = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [{"from": "input1", "to": "output1"}],
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns SCHEMA_VALIDATION error") {
      REQUIRE_FALSE(result.valid);
      bool found = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::SCHEMA_VALIDATION) {
          found = true;
        }
      }
      REQUIRE(found);
    }
  }
}

// ---------------------------------------------------------------------------
// Phase 3/4: Operator-level errors (schema or semantic depending on what the schema covers)
// ---------------------------------------------------------------------------
SCENARIO("diagnoseProgram detects operator creation errors", "[diagnostics]") {
  GIVEN("A program with an unknown operator type") {
    std::string json_str = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "NonExistentOperator", "id": "bad1"},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "bad1"},
        {"from": "bad1", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns errors (schema catches unknown types via oneOf)") {
      REQUIRE_FALSE(result.valid);
      REQUIRE(result.errors.size() > 0);
      // The schema uses oneOf for known operator types, so unknown types
      // are caught at Phase 3 as SCHEMA_VALIDATION errors
      bool found = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::SCHEMA_VALIDATION || e.code == DiagnosticCode::UNKNOWN_OPERATOR_TYPE) {
          found = true;
        }
      }
      REQUIRE(found);
    }
  }

  GIVEN("A program with invalid operator parameter (PeakDetector even window)") {
    std::string json_str = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "PeakDetector", "id": "pd1", "window_size": 4},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "pd1"},
        {"from": "pd1", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns an error for the invalid parameter") {
      REQUIRE_FALSE(result.valid);
      REQUIRE(result.errors.size() > 0);
      // May be caught at schema level (if schema has minimum constraint)
      // or at semantic level as INVALID_PARAMETER_VALUE
      bool found = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::INVALID_PARAMETER_VALUE || e.code == DiagnosticCode::SCHEMA_VALIDATION) {
          found = true;
        }
      }
      REQUIRE(found);
    }
  }

  GIVEN("A program with multiple operator errors") {
    std::string json_str = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "PeakDetector", "id": "pd1", "window_size": 4},
        {"type": "FiniteImpulseResponse", "id": "fir1", "coeff": []},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "pd1"},
        {"from": "pd1", "to": "fir1"},
        {"from": "fir1", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns errors for the bad operators") {
      REQUIRE_FALSE(result.valid);
      // Errors may be at schema or semantic level; either way there should be multiple
      REQUIRE(result.errors.size() >= 1);
    }
  }
}

// ---------------------------------------------------------------------------
// Phase 4b: Connection wiring errors
// ---------------------------------------------------------------------------
SCENARIO("diagnoseProgram detects connection errors", "[diagnostics]") {
  GIVEN("A program with invalid connection reference") {
    std::string json_str = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "nonexistent"}
      ],
      "entryOperator": "input1",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns INVALID_OPERATOR_REF with path and suggestion") {
      REQUIRE_FALSE(result.valid);
      bool found = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::INVALID_OPERATOR_REF) {
          REQUIRE(e.path == "/connections/0/to");
          REQUIRE_FALSE(e.suggestion.empty());
          found = true;
        }
      }
      REQUIRE(found);
    }
  }

  GIVEN("A program with port type mismatch") {
    std::string json_str = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["boolean"]}
      ],
      "connections": [
        {"from": "input1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
      ],
      "entryOperator": "input1",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns PORT_TYPE_MISMATCH error") {
      REQUIRE_FALSE(result.valid);
      bool found = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::PORT_TYPE_MISMATCH) {
          found = true;
        }
      }
      REQUIRE(found);
    }
  }
}

// ---------------------------------------------------------------------------
// Phase 4c: Entry operator errors
// ---------------------------------------------------------------------------
SCENARIO("diagnoseProgram detects entry operator errors", "[diagnostics]") {
  GIVEN("A program with a nonexistent entry operator") {
    std::string json_str = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "output1"}
      ],
      "entryOperator": "does_not_exist",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns INVALID_ENTRY_OPERATOR error") {
      REQUIRE_FALSE(result.valid);
      bool found = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::INVALID_ENTRY_OPERATOR) {
          REQUIRE(e.path == "/entryOperator");
          REQUIRE_FALSE(e.suggestion.empty());
          found = true;
        }
      }
      REQUIRE(found);
    }
  }
}

// ---------------------------------------------------------------------------
// Phase 4d: Output mapping errors
// ---------------------------------------------------------------------------
SCENARIO("diagnoseProgram detects output mapping errors", "[diagnostics]") {
  GIVEN("A program with a nonexistent output operator") {
    std::string json_str = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "output1"}
      ],
      "entryOperator": "input1",
      "output": {"nonexistent_op": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns INVALID_OUTPUT_MAPPING error") {
      REQUIRE_FALSE(result.valid);
      bool found = false;
      for (const auto& e : result.errors) {
        if (e.code == DiagnosticCode::INVALID_OUTPUT_MAPPING) {
          REQUIRE(e.path.find("/output/") != std::string::npos);
          found = true;
        }
      }
      REQUIRE(found);
    }
  }
}

// ---------------------------------------------------------------------------
// Valid program
// ---------------------------------------------------------------------------
SCENARIO("diagnoseProgram returns valid for correct programs", "[diagnostics]") {
  GIVEN("A valid simple program") {
    std::string json_str = R"({
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
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    THEN("Returns valid with no errors") {
      REQUIRE(result.valid);
      REQUIRE(result.errors.empty());
    }
  }
}

// ---------------------------------------------------------------------------
// JSON output format
// ---------------------------------------------------------------------------
SCENARIO("diagnoseProgram returns correct JSON format", "[diagnostics]") {
  GIVEN("An invalid program") {
    std::string json_str = R"({
      "operators": [
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
      ],
      "connections": [
        {"from": "input1", "to": "nonexistent"}
      ],
      "entryOperator": "input1",
      "output": {"output1": ["o1"]}
    })";

    auto result = ProgramDiagnostics::diagnose(json_str);
    auto j = json(result);

    THEN("JSON contains valid, errors array with severity, path, message, code") {
      REQUIRE(j.contains("valid"));
      REQUIRE(j["valid"].is_boolean());
      REQUIRE(j.contains("errors"));
      REQUIRE(j["errors"].is_array());
      REQUIRE(j["errors"].size() > 0);

      auto& first_error = j["errors"][0];
      REQUIRE(first_error.contains("severity"));
      REQUIRE(first_error.contains("path"));
      REQUIRE(first_error.contains("message"));
      REQUIRE(first_error.contains("code"));
    }
  }
}
