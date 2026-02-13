#ifndef RTBOT_DIAGNOSTICS_H
#define RTBOT_DIAGNOSTICS_H

#include <map>
#include <memory>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "rtbot/OperatorJson.h"
#include "rtbot/Prototype.h"
#include "rtbot/jsonschema.hpp"

namespace rtbot {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Error codes
// ---------------------------------------------------------------------------
enum class DiagnosticCode {
  // Phase 1: JSON parse
  INVALID_JSON,

  // Phase 2: Prototype resolution
  UNKNOWN_PROTOTYPE,
  UNKNOWN_PARAMETER,
  MISSING_PARAMETER,
  INVALID_PARAMETER_TYPE,
  UNKNOWN_PARAM_REFERENCE,

  // Phase 3: Schema validation
  SCHEMA_VALIDATION,

  // Phase 4: Semantic
  UNKNOWN_OPERATOR_TYPE,
  INVALID_PORT_TYPE,
  INVALID_PARAMETER_VALUE,
  MISSING_REQUIRED_FIELD,

  INVALID_OPERATOR_REF,
  PORT_INDEX_OUT_OF_BOUNDS,
  PORT_TYPE_MISMATCH,

  INVALID_ENTRY_OPERATOR,
  ENTRY_PORT_MISMATCH,

  MISSING_OUTPUT_FIELD,
  INVALID_OUTPUT_MAPPING,
};

inline std::string diagnostic_code_to_string(DiagnosticCode code) {
  switch (code) {
    case DiagnosticCode::INVALID_JSON:
      return "INVALID_JSON";
    case DiagnosticCode::UNKNOWN_PROTOTYPE:
      return "UNKNOWN_PROTOTYPE";
    case DiagnosticCode::UNKNOWN_PARAMETER:
      return "UNKNOWN_PARAMETER";
    case DiagnosticCode::MISSING_PARAMETER:
      return "MISSING_PARAMETER";
    case DiagnosticCode::INVALID_PARAMETER_TYPE:
      return "INVALID_PARAMETER_TYPE";
    case DiagnosticCode::UNKNOWN_PARAM_REFERENCE:
      return "UNKNOWN_PARAM_REFERENCE";
    case DiagnosticCode::SCHEMA_VALIDATION:
      return "SCHEMA_VALIDATION";
    case DiagnosticCode::UNKNOWN_OPERATOR_TYPE:
      return "UNKNOWN_OPERATOR_TYPE";
    case DiagnosticCode::INVALID_PORT_TYPE:
      return "INVALID_PORT_TYPE";
    case DiagnosticCode::INVALID_PARAMETER_VALUE:
      return "INVALID_PARAMETER_VALUE";
    case DiagnosticCode::MISSING_REQUIRED_FIELD:
      return "MISSING_REQUIRED_FIELD";
    case DiagnosticCode::INVALID_OPERATOR_REF:
      return "INVALID_OPERATOR_REF";
    case DiagnosticCode::PORT_INDEX_OUT_OF_BOUNDS:
      return "PORT_INDEX_OUT_OF_BOUNDS";
    case DiagnosticCode::PORT_TYPE_MISMATCH:
      return "PORT_TYPE_MISMATCH";
    case DiagnosticCode::INVALID_ENTRY_OPERATOR:
      return "INVALID_ENTRY_OPERATOR";
    case DiagnosticCode::ENTRY_PORT_MISMATCH:
      return "ENTRY_PORT_MISMATCH";
    case DiagnosticCode::MISSING_OUTPUT_FIELD:
      return "MISSING_OUTPUT_FIELD";
    case DiagnosticCode::INVALID_OUTPUT_MAPPING:
      return "INVALID_OUTPUT_MAPPING";
  }
  return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// Diagnostic structs
// ---------------------------------------------------------------------------
struct DiagnosticError {
  std::string severity;    // "error" or "warning"
  std::string path;        // JSON pointer, e.g. "/operators/2/window_size"
  std::string message;     // Human-readable explanation
  DiagnosticCode code;     // Programmatic error code
  std::string suggestion;  // Optional remediation hint (may be empty)
};

inline void to_json(json& j, const DiagnosticError& e) {
  j = json{
      {"severity", e.severity},
      {"path", e.path},
      {"message", e.message},
      {"code", diagnostic_code_to_string(e.code)},
  };
  if (!e.suggestion.empty()) {
    j["suggestion"] = e.suggestion;
  }
}

struct DiagnosticResult {
  bool valid;
  std::vector<DiagnosticError> errors;
};

inline void to_json(json& j, const DiagnosticResult& r) {
  j = json{{"valid", r.valid}, {"errors", r.errors}};
}

// ---------------------------------------------------------------------------
// ProgramDiagnostics
// ---------------------------------------------------------------------------
class ProgramDiagnostics {
 public:
  static DiagnosticResult diagnose(const std::string& json_string) {
    DiagnosticResult result;
    result.valid = true;

    // Phase 1: JSON Parse
    json j;
    if (!phase_json_parse(json_string, j, result)) {
      result.valid = false;
      return result;
    }

    // Phase 2: Prototype Resolution
    if (!phase_prototype_resolution(j, result)) {
      result.valid = false;
      return result;
    }

    // Phase 3: Schema Validation
    if (!phase_schema_validation(j, result)) {
      result.valid = false;
      return result;
    }

    // Phase 4: Semantic Validation
    std::map<std::string, std::shared_ptr<Operator>> operators;

    // 4a: Operator creation
    phase_operator_creation(j, operators, result);
    bool had_operator_errors = !result.errors.empty();

    // 4b: Connection wiring (only if all operators created successfully)
    if (!had_operator_errors) {
      phase_connection_wiring(j, operators, result);
    }

    // 4c: Entry operator validation
    phase_entry_operator(j, operators, result);

    // 4d: Output mappings validation
    phase_output_mappings(j, operators, result);

    result.valid = result.errors.empty();
    return result;
  }

 private:
  // -------------------------------------------------------------------------
  // Utility helpers
  // -------------------------------------------------------------------------
  static std::string join_vec(const std::vector<std::string>& v) {
    std::string result;
    for (size_t i = 0; i < v.size(); i++) {
      if (i > 0) result += ", ";
      result += v[i];
    }
    return result;
  }

  static std::vector<std::string> get_operator_ids(const std::map<std::string, std::shared_ptr<Operator>>& operators) {
    std::vector<std::string> ids;
    for (const auto& kv : operators) {
      ids.push_back(kv.first);
    }
    return ids;
  }

  static bool check_param_type(const std::string& type, const json& value) {
    if (type == "number") return value.is_number();
    if (type == "string") return value.is_string();
    if (type == "boolean") return value.is_boolean();
    if (type == "array") return value.is_array();
    if (type == "object") return value.is_object();
    return false;
  }

  // C++17-compatible suffix check (no std::string::ends_with)
  static bool ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  static std::shared_ptr<Operator> resolve_operator(
      const std::string& id, const std::map<std::string, std::shared_ptr<Operator>>& operators) {
    auto it = operators.find(id);
    if (it != operators.end()) return it->second;

    std::string suffix = "::" + id;
    for (const auto& kv : operators) {
      if (ends_with(kv.first, suffix) || kv.first == id) {
        return kv.second;
      }
    }
    return nullptr;
  }

  // -------------------------------------------------------------------------
  // Error classification helpers
  // -------------------------------------------------------------------------
  static DiagnosticCode classify_operator_error(const std::string& msg) {
    if (msg.find("Unknown operator type") != std::string::npos) return DiagnosticCode::UNKNOWN_OPERATOR_TYPE;
    if (msg.find("port type") != std::string::npos || msg.find("Invalid input port type") != std::string::npos ||
        msg.find("Invalid output port type") != std::string::npos)
      return DiagnosticCode::INVALID_PORT_TYPE;
    if (msg.find("must have id and type") != std::string::npos ||
        msg.find("must specify from and to") != std::string::npos ||
        msg.find("must have at least") != std::string::npos ||
        msg.find("must contain at least") != std::string::npos)
      return DiagnosticCode::MISSING_REQUIRED_FIELD;
    if (msg.find("cannot be empty") != std::string::npos || msg.find("must be at least") != std::string::npos ||
        msg.find("must be odd") != std::string::npos || msg.find("requires at least") != std::string::npos ||
        msg.find("must be positive") != std::string::npos || msg.find("At least") != std::string::npos)
      return DiagnosticCode::INVALID_PARAMETER_VALUE;
    if (msg.find("Invalid port name format") != std::string::npos) return DiagnosticCode::PORT_INDEX_OUT_OF_BOUNDS;
    if (msg.find("Entry operator not found") != std::string::npos) return DiagnosticCode::INVALID_ENTRY_OPERATOR;
    if (msg.find("Entry operator has less data ports") != std::string::npos) return DiagnosticCode::ENTRY_PORT_MISMATCH;
    if (msg.find("Output operator not found") != std::string::npos) return DiagnosticCode::INVALID_OUTPUT_MAPPING;
    if (msg.find("Invalid pipeline output port") != std::string::npos) return DiagnosticCode::INVALID_OUTPUT_MAPPING;
    if (msg.find("invalid operator reference") != std::string::npos) return DiagnosticCode::INVALID_OPERATOR_REF;
    if (msg.find("type mismatch") != std::string::npos) return DiagnosticCode::PORT_TYPE_MISMATCH;
    if (msg.find("Invalid output port index") != std::string::npos ||
        msg.find("Invalid child data port") != std::string::npos ||
        msg.find("Invalid child control port") != std::string::npos)
      return DiagnosticCode::PORT_INDEX_OUT_OF_BOUNDS;
    return DiagnosticCode::INVALID_PARAMETER_VALUE;
  }

  static std::string refine_operator_error_path(const std::string& base_path, const std::string& msg,
                                                 const json& op_json) {
    if ((msg.find("coefficients") != std::string::npos || msg.find("FIR") != std::string::npos) &&
        op_json.contains("coeff"))
      return base_path + "/coeff";
    if (msg.find("coefficient") != std::string::npos && op_json.contains("coefficients"))
      return base_path + "/coefficients";
    if (msg.find("b_coeffs") != std::string::npos || msg.find("a_coeffs") != std::string::npos) {
      if (op_json.contains("b_coeffs")) return base_path + "/b_coeffs";
      if (op_json.contains("a_coeffs")) return base_path + "/a_coeffs";
    }
    if (msg.find("window size") != std::string::npos || msg.find("window_size") != std::string::npos)
      return base_path + "/window_size";
    if (msg.find("port type") != std::string::npos && op_json.contains("portTypes"))
      return base_path + "/portTypes";
    if (msg.find("port type") != std::string::npos && op_json.contains("input_port_types"))
      return base_path + "/input_port_types";
    if (msg.find("interval") != std::string::npos || msg.find("Time interval") != std::string::npos ||
        msg.find("Resampling interval") != std::string::npos)
      return base_path + "/interval";
    if (msg.find("points") != std::string::npos || msg.find("interpolation") != std::string::npos)
      return base_path + "/points";
    if (msg.find("Entry operator") != std::string::npos) return base_path + "/entryOperator";
    return base_path;
  }

  static std::string generate_operator_suggestion(DiagnosticCode code, const std::string& msg) {
    switch (code) {
      case DiagnosticCode::UNKNOWN_OPERATOR_TYPE:
        return "Available types include: Input, Output, MovingAverage, StandardDeviation, "
               "FiniteImpulseResponse, InfiniteImpulseResponse, PeakDetector, Linear, "
               "Scale, Add, Power, Pipeline, Join, Function, Identity, Difference, etc.";
      case DiagnosticCode::INVALID_PORT_TYPE:
        return "Valid port types: number, boolean, vector_number, vector_boolean";
      case DiagnosticCode::INVALID_PARAMETER_VALUE:
        if (msg.find("PeakDetector") != std::string::npos && msg.find("odd") != std::string::npos)
          return "Use an odd number >= 3 (e.g., 3, 5, 7)";
        if (msg.find("PeakDetector") != std::string::npos && msg.find("at least 3") != std::string::npos)
          return "Window size must be an odd number >= 3";
        if (msg.find("FIR") != std::string::npos || msg.find("cannot be empty") != std::string::npos)
          return "Provide at least one coefficient";
        if (msg.find("interval") != std::string::npos) return "Interval must be a positive integer";
        break;
      default:
        break;
    }
    return "";
  }

  // -------------------------------------------------------------------------
  // Phase 1: JSON Parse
  // -------------------------------------------------------------------------
  static bool phase_json_parse(const std::string& input, json& parsed, DiagnosticResult& result) {
    try {
      parsed = json::parse(input);
      return true;
    } catch (const json::parse_error& e) {
      result.errors.push_back({"error", "", std::string("Invalid JSON: ") + e.what(), DiagnosticCode::INVALID_JSON,
                                "Ensure the input is valid JSON"});
      return false;
    }
  }

  // -------------------------------------------------------------------------
  // Phase 2: Prototype Resolution
  // -------------------------------------------------------------------------
  static bool phase_prototype_resolution(json& j, DiagnosticResult& result) {
    if (!j.contains("prototypes")) {
      return true;
    }

    std::vector<DiagnosticError> errors;

    if (j.contains("operators") && j["operators"].is_array()) {
      for (size_t i = 0; i < j["operators"].size(); i++) {
        const auto& op = j["operators"][i];
        if (!op.contains("prototype")) continue;

        std::string proto_name = op["prototype"].get<std::string>();
        std::string instance_id = op.value("id", "<unknown>");
        std::string base_path = "/operators/" + std::to_string(i);

        // Check prototype exists
        if (!j["prototypes"].contains(proto_name)) {
          std::vector<std::string> available;
          for (auto it = j["prototypes"].begin(); it != j["prototypes"].end(); ++it) {
            available.push_back(it.key());
          }
          errors.push_back({"error", base_path + "/prototype",
                            "Unknown prototype: " + proto_name, DiagnosticCode::UNKNOWN_PROTOTYPE,
                            "Available prototypes: " + join_vec(available)});
          continue;
        }

        const auto& proto_def = j["prototypes"][proto_name];
        if (!proto_def.contains("parameters")) continue;

        // Build parameter lookup
        std::map<std::string, std::string> param_types;
        std::vector<std::string> required_params;
        std::vector<std::string> all_param_names;
        for (const auto& p : proto_def["parameters"]) {
          std::string pname = p["name"].get<std::string>();
          param_types[pname] = p["type"].get<std::string>();
          all_param_names.push_back(pname);
          if (!p.contains("default") || p["default"].is_null()) {
            required_params.push_back(pname);
          }
        }

        json provided = op.value("parameters", json::object());

        // Check for unknown parameters
        for (auto it = provided.begin(); it != provided.end(); ++it) {
          if (param_types.find(it.key()) == param_types.end()) {
            errors.push_back({"error", base_path + "/parameters/" + it.key(),
                              "Unknown parameter '" + it.key() + "' in prototype instance '" + instance_id + "'",
                              DiagnosticCode::UNKNOWN_PARAMETER,
                              "Available parameters: " + join_vec(all_param_names)});
          }
        }

        // Check for missing required parameters
        for (const auto& req : required_params) {
          if (!provided.contains(req)) {
            errors.push_back({"error", base_path + "/parameters",
                              "Missing required parameter '" + req + "' in prototype instance '" + instance_id + "'",
                              DiagnosticCode::MISSING_PARAMETER, ""});
          }
        }

        // Type-check provided parameters
        for (auto it = provided.begin(); it != provided.end(); ++it) {
          auto pt = param_types.find(it.key());
          if (pt == param_types.end()) continue;
          if (!check_param_type(pt->second, it.value())) {
            errors.push_back({"error", base_path + "/parameters/" + it.key(),
                              "Parameter '" + it.key() + "' must be a " + pt->second,
                              DiagnosticCode::INVALID_PARAMETER_TYPE, ""});
          }
        }
      }
    }

    if (!errors.empty()) {
      result.errors.insert(result.errors.end(), errors.begin(), errors.end());
      return false;
    }

    // Pre-validation passed; actually resolve prototypes
    try {
      PrototypeHandler::resolve_prototypes(j);
      return true;
    } catch (const std::exception& e) {
      std::string msg = e.what();
      DiagnosticCode code = DiagnosticCode::UNKNOWN_PARAM_REFERENCE;
      if (msg.find("Unknown parameter '") != std::string::npos)
        code = DiagnosticCode::UNKNOWN_PARAMETER;
      else if (msg.find("Missing required parameter") != std::string::npos)
        code = DiagnosticCode::MISSING_PARAMETER;
      else if (msg.find("must be a") != std::string::npos)
        code = DiagnosticCode::INVALID_PARAMETER_TYPE;

      result.errors.push_back({"error", "", msg, code, ""});
      return false;
    }
  }

  // -------------------------------------------------------------------------
  // Phase 3: Schema Validation
  // -------------------------------------------------------------------------
  class DiagnosticSchemaHandler : public nlohmann::json_schema::basic_error_handler {
   public:
    std::vector<DiagnosticError> errors;

    void error(const nlohmann::json::json_pointer& ptr, const nlohmann::json& /*instance*/,
               const std::string& message) override {
      nlohmann::json_schema::basic_error_handler::error(ptr, {}, message);
      errors.push_back({"error", ptr.to_string(), message, DiagnosticCode::SCHEMA_VALIDATION, ""});
    }
  };

  static bool phase_schema_validation(const json& j, DiagnosticResult& result) {
    nlohmann::json_schema::json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);

    try {
      validator.set_root_schema(rtbot_schema);
    } catch (const std::exception& e) {
      result.errors.push_back(
          {"error", "", std::string("Schema setup error: ") + e.what(), DiagnosticCode::SCHEMA_VALIDATION, ""});
      return false;
    }

    DiagnosticSchemaHandler handler;
    validator.validate(j, handler);

    if (!handler.errors.empty()) {
      result.errors.insert(result.errors.end(), handler.errors.begin(), handler.errors.end());
      return false;
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // Phase 4a: Operator Creation
  // -------------------------------------------------------------------------
  static void phase_operator_creation(const json& j, std::map<std::string, std::shared_ptr<Operator>>& operators,
                                      DiagnosticResult& result) {
    if (!j.contains("operators") || !j["operators"].is_array()) return;

    const auto& ops = j["operators"];
    for (size_t i = 0; i < ops.size(); i++) {
      const auto& op_json = ops[i];
      std::string base_path = "/operators/" + std::to_string(i);
      std::string op_id = op_json.value("id", "<unknown>");

      try {
        auto op = OperatorJson::read_op(op_json.dump());
        std::string qualified_id = op_id;
        operators[qualified_id] = op;

        // For Pipelines, also register internal operators with qualified IDs
        if (auto pipeline = std::dynamic_pointer_cast<Pipeline>(op)) {
          register_pipeline_operators(qualified_id, pipeline, operators);
        }
      } catch (const std::exception& e) {
        std::string msg = e.what();
        DiagnosticCode code = classify_operator_error(msg);
        std::string path = refine_operator_error_path(base_path, msg, op_json);
        std::string suggestion = generate_operator_suggestion(code, msg);

        result.errors.push_back({"error", path, msg, code, suggestion});
      }
    }
  }

  static void register_pipeline_operators(const std::string& parent_prefix,
                                          const std::shared_ptr<Pipeline>& pipeline,
                                          std::map<std::string, std::shared_ptr<Operator>>& operators) {
    for (const auto& kv : pipeline->get_operators()) {
      std::string qualified_id = parent_prefix + "::" + kv.first;
      operators[qualified_id] = kv.second;

      if (auto nested_pipeline = std::dynamic_pointer_cast<Pipeline>(kv.second)) {
        register_pipeline_operators(qualified_id, nested_pipeline, operators);
      }
    }
  }

  // -------------------------------------------------------------------------
  // Phase 4b: Connection Wiring
  // -------------------------------------------------------------------------
  static void phase_connection_wiring(const json& j, std::map<std::string, std::shared_ptr<Operator>>& operators,
                                      DiagnosticResult& result) {
    if (!j.contains("connections") || !j["connections"].is_array()) return;

    std::vector<std::string> known_ids = get_operator_ids(operators);
    const auto& conns = j["connections"];

    for (size_t i = 0; i < conns.size(); i++) {
      const auto& conn = conns[i];
      std::string base_path = "/connections/" + std::to_string(i);
      std::string from_id = conn.value("from", "");
      std::string to_id = conn.value("to", "");

      auto from_op = resolve_operator(from_id, operators);
      auto to_op = resolve_operator(to_id, operators);

      if (!from_op) {
        result.errors.push_back({"error", base_path + "/from",
                                 "Referenced operator \"" + from_id + "\" does not exist",
                                 DiagnosticCode::INVALID_OPERATOR_REF,
                                 "Available operators: " + join_vec(known_ids)});
      }
      if (!to_op) {
        result.errors.push_back({"error", base_path + "/to",
                                 "Referenced operator \"" + to_id + "\" does not exist",
                                 DiagnosticCode::INVALID_OPERATOR_REF,
                                 "Available operators: " + join_vec(known_ids)});
      }

      if (!from_op || !to_op) continue;

      // Try connecting to detect port errors
      try {
        auto from_port = OperatorJson::parse_port_name(conn.value("fromPort", "o1"));
        auto to_port = OperatorJson::parse_port_name(conn.value("toPort", "i1"));
        PortKind to_kind = to_port.kind;
        if (conn.contains("toPortType")) {
          to_kind = conn["toPortType"] == "control" ? PortKind::CONTROL : PortKind::DATA;
        }
        from_op->connect(to_op, from_port.index, to_port.index, to_kind);
      } catch (const std::exception& e) {
        std::string msg = e.what();
        DiagnosticCode code = DiagnosticCode::PORT_TYPE_MISMATCH;
        std::string path = base_path;

        if (msg.find("Invalid output port index") != std::string::npos) {
          code = DiagnosticCode::PORT_INDEX_OUT_OF_BOUNDS;
          path = base_path + "/fromPort";
        } else if (msg.find("Invalid child data port") != std::string::npos ||
                   msg.find("Invalid child control port") != std::string::npos) {
          code = DiagnosticCode::PORT_INDEX_OUT_OF_BOUNDS;
          path = base_path + "/toPort";
        } else if (msg.find("type mismatch") != std::string::npos) {
          code = DiagnosticCode::PORT_TYPE_MISMATCH;
        } else if (msg.find("Invalid port name format") != std::string::npos) {
          code = DiagnosticCode::PORT_INDEX_OUT_OF_BOUNDS;
        }

        result.errors.push_back({"error", path, msg, code, ""});
      }
    }
  }

  // -------------------------------------------------------------------------
  // Phase 4c: Entry Operator Validation
  // -------------------------------------------------------------------------
  static void phase_entry_operator(const json& j,
                                   const std::map<std::string, std::shared_ptr<Operator>>& operators,
                                   DiagnosticResult& result) {
    if (!j.contains("entryOperator")) {
      // Schema should catch this, but be defensive
      return;
    }

    std::string entry_id = j["entryOperator"].get<std::string>();
    auto op = resolve_operator(entry_id, operators);

    if (!op) {
      std::vector<std::string> ids = get_operator_ids(operators);
      result.errors.push_back({"error", "/entryOperator", "Entry operator not found: " + entry_id,
                                DiagnosticCode::INVALID_ENTRY_OPERATOR,
                                "Available operators: " + join_vec(ids)});
    }
  }

  // -------------------------------------------------------------------------
  // Phase 4d: Output Mappings Validation
  // -------------------------------------------------------------------------
  static void phase_output_mappings(const json& j,
                                    const std::map<std::string, std::shared_ptr<Operator>>& operators,
                                    DiagnosticResult& result) {
    if (!j.contains("output")) {
      result.errors.push_back(
          {"error", "", "Program JSON must contain 'output' field", DiagnosticCode::MISSING_OUTPUT_FIELD, ""});
      return;
    }

    const auto& output = j["output"];
    for (auto it = output.begin(); it != output.end(); ++it) {
      std::string op_id = it.key();
      std::string path = "/output/" + op_id;

      auto op = resolve_operator(op_id, operators);
      if (!op) {
        std::vector<std::string> ids = get_operator_ids(operators);
        result.errors.push_back({"error", path, "Output operator not found: " + op_id,
                                  DiagnosticCode::INVALID_OUTPUT_MAPPING,
                                  "Available operators: " + join_vec(ids)});
        continue;
      }

      // Validate port references
      if (it.value().is_array()) {
        for (size_t pi = 0; pi < it.value().size(); pi++) {
          std::string port_str = it.value()[pi].get<std::string>();
          try {
            OperatorJson::parse_port_name(port_str);
          } catch (const std::exception& e) {
            result.errors.push_back({"error", path + "/" + std::to_string(pi),
                                     "Invalid port reference '" + port_str + "': " + e.what(),
                                     DiagnosticCode::INVALID_OUTPUT_MAPPING, "Use format like 'o1', 'o2', etc."});
          }
        }
      }
    }
  }
};

}  // namespace rtbot

#endif  // RTBOT_DIAGNOSTICS_H
