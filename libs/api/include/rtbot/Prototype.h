#ifndef RTBOT_PROTOTYPE_H
#define RTBOT_PROTOTYPE_H

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "rtbot/Pipeline.h"

namespace rtbot {

using json = nlohmann::json;

struct PrototypeParameter {
  std::string name;
  std::string type;
  json default_value;
};

struct PrototypePort {
  std::string operator_id;
  std::string port_id = "i1";
};

struct PrototypeDefinition {
  std::vector<PrototypeParameter> parameters;
  json operators;
  json connections;
  PrototypePort entry;
  PrototypePort output;
};

class PrototypeHandler {
 public:
  static void resolve_prototypes(json& program_json) {
    if (!program_json.contains("prototypes")) {
      return;
    }

    // Parse and store all prototypes
    std::map<std::string, PrototypeDefinition> prototypes = parse_prototypes(program_json["prototypes"]);

    json expanded_operators = json::array();
    json expanded_connections = json::array();
    std::map<std::string, std::pair<std::string, std::string>> instance_mappings;

    // Process original operators
    for (const auto& op : program_json["operators"]) {
      if (op.contains("prototype")) {
        // Handle prototype instance
        const auto& proto = prototypes.at(op["prototype"].get<std::string>());
        const std::string instance_id = op["id"].get<std::string>();

        expand_prototype_instance(instance_id, proto, op.value("parameters", json::object()), expanded_operators,
                                  expanded_connections, instance_mappings, prototypes);
      } else {
        // Regular operator - add as-is
        expanded_operators.push_back(op);
      }
    }

    // Process original connections
    for (const auto& conn : program_json["connections"]) {
      bool processed = false;

      // Check if connection involves prototype instances
      for (const auto& [instance_id, mapping] : instance_mappings) {
        if (conn["from"] == instance_id) {
          json new_conn = conn;
          new_conn["from"] = mapping.second;  // Use output port
          expanded_connections.push_back(new_conn);
          processed = true;
        }
        if (conn["to"] == instance_id) {
          json new_conn = conn;
          new_conn["to"] = mapping.first;  // Use entry port
          expanded_connections.push_back(new_conn);
          processed = true;
        }
      }

      // If not involving prototypes, add as-is
      if (!processed) {
        expanded_connections.push_back(conn);
      }
    }

    // Update program JSON
    program_json["operators"] = expanded_operators;
    program_json["connections"] = expanded_connections;
    program_json.erase("prototypes");
  }

 private:
  static std::map<std::string, PrototypeDefinition> parse_prototypes(const json& prototypes) {
    std::map<std::string, PrototypeDefinition> result;

    for (auto it = prototypes.begin(); it != prototypes.end(); ++it) {
      PrototypeDefinition proto;

      if (it.value().contains("parameters")) {
        for (const auto& param : it.value()["parameters"]) {
          proto.parameters.push_back({param["name"].get<std::string>(), param["type"].get<std::string>(),
                                      param.contains("default") ? param["default"] : json(nullptr)});
        }
      }

      proto.operators = it.value()["operators"];
      proto.connections = it.value()["connections"];

      const auto& entry = it.value()["entry"];
      proto.entry.operator_id = entry["operator"];
      proto.entry.port_id = entry.value("port", "i1");

      const auto& output = it.value()["output"];
      proto.output.operator_id = output["operator"];
      proto.output.port_id = output.value("port", "o1");

      result[it.key()] = std::move(proto);
    }

    return result;
  }

  static void resolve_pipeline_operators(json& operators, const json& params) {
    for (auto& op : operators) {
      if (op["type"] == "Pipeline") {
        // Recursively resolve parameters in nested Pipeline operators
        if (op.contains("operators")) {
          resolve_pipeline_operators(op["operators"], params);
        }
        // Handle outputMappings
        if (op.contains("outputMappings")) {
          resolve_parameter_references(op["outputMappings"], params);
        }
      }
      // Always resolve parameters in the operator itself
      resolve_parameter_references(op, params);
    }
  }

  static void expand_prototype_instance(
      const std::string& instance_id, const PrototypeDefinition& proto, const json& params, json& expanded_operators,
      json& expanded_connections, std::map<std::string, std::pair<std::string, std::string>>& instance_mappings,
      const std::map<std::string, PrototypeDefinition>& all_prototypes  // Add prototypes map
  ) {
    json resolved_params = resolve_parameters(instance_id, params, proto.parameters);

    json instance_operators = proto.operators;

    // First pass: Resolve parameters
    resolve_pipeline_operators(instance_operators, resolved_params);

    // Second pass: Handle nested prototypes
    for (auto& op : instance_operators) {
      if (op.contains("prototype")) {
        // Recursively expand nested prototype
        const auto& nested_proto = all_prototypes.at(op["prototype"].get<std::string>());
        const std::string nested_id = instance_id + "::" + op["id"].get<std::string>();

        expand_prototype_instance(nested_id, nested_proto, op.value("parameters", json::object()), expanded_operators,
                                  expanded_connections, instance_mappings, all_prototypes);
      } else {
        // Regular operator - add with scoped ID
        std::string local_id = op["id"];
        op["id"] = instance_id + "::" + local_id;
        expanded_operators.push_back(op);
      }
    }

    // Expand connections with scoped IDs
    for (auto conn : proto.connections) {
      conn["from"] = instance_id + "::" + conn["from"].get<std::string>();
      conn["to"] = instance_id + "::" + conn["to"].get<std::string>();
      expanded_connections.push_back(conn);
    }

    instance_mappings[instance_id] =
        std::make_pair(instance_id + "::" + proto.entry.operator_id, instance_id + "::" + proto.output.operator_id);
  }

  static json resolve_parameters(const std::string& instance_id, const json& provided_params,
                                 const std::vector<PrototypeParameter>& proto_params) {
    json resolved;

    // Apply provided values
    for (auto it = provided_params.begin(); it != provided_params.end(); ++it) {
      auto param_it =
          std::find_if(proto_params.begin(), proto_params.end(), [&](const auto& p) { return p.name == it.key(); });

      if (param_it == proto_params.end()) {
        throw std::runtime_error("Unknown parameter '" + it.key() + "' in prototype instance '" + instance_id + "'");
      }

      validate_parameter_type(it.key(), param_it->type, it.value());
      resolved[it.key()] = it.value();
    }

    // Fill in defaults
    for (const auto& param : proto_params) {
      if (!resolved.contains(param.name)) {
        if (param.default_value.is_null()) {
          throw std::runtime_error("Missing required parameter '" + param.name + "' in prototype instance '" +
                                   instance_id + "'");
        }
        resolved[param.name] = param.default_value;
      }
    }

    return resolved;
  }

  static void validate_parameter_type(const std::string& param_name, const std::string& param_type, const json& value) {
    bool valid = false;
    if (param_type == "number")
      valid = value.is_number();
    else if (param_type == "string")
      valid = value.is_string();
    else if (param_type == "boolean")
      valid = value.is_boolean();
    else if (param_type == "array")
      valid = value.is_array();
    else if (param_type == "object")
      valid = value.is_object();

    if (!valid) {
      throw std::runtime_error("Parameter '" + param_name + "' must be a " + param_type);
    }
  }

  static void resolve_parameter_references(json& config, const json& params) {
    if (config.is_object()) {
      for (auto it = config.begin(); it != config.end(); ++it) {
        if (it.value().is_string()) {
          std::string value = it.value();
          if (value.size() > 3 && value.substr(0, 2) == "${" && value.back() == '}') {
            std::string param_name = value.substr(2, value.length() - 3);
            if (!params.contains(param_name)) {
              throw std::runtime_error("Unknown parameter reference '${" + param_name + "}'");
            }
            it.value() = params[param_name];
          }
        } else if (it.value().is_object() || it.value().is_array()) {
          resolve_parameter_references(it.value(), params);
        }
      }
    } else if (config.is_array()) {
      for (auto& element : config) {
        resolve_parameter_references(element, params);
      }
    }
  }
};

}  // namespace rtbot

#endif  // RTBOT_PROTOTYPE_H