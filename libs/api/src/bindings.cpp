#ifndef RTBOT_BINDINGS_H
#define RTBOT_BINDINGS_H

#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Program.h"
#include "rtbot/jsonschema.hpp"

namespace rtbot {

using json = nlohmann::json;

void to_json(json& j, const ProgramMsgBatch& batch) {
  j = json::object();
  for (const auto& [op_id, op_batch] : batch) {
    j[op_id] = json::object();
    for (const auto& [port_name, msgs] : op_batch) {
      j[op_id][port_name] = json::array();
      for (const auto& msg : msgs) {
        if (auto* num_msg = dynamic_cast<const Message<NumberData>*>(msg.get())) {
          j[op_id][port_name].push_back({{"time", num_msg->time}, {"value", num_msg->data.value}});
        }
        // Add other message type conversions as needed
      }
    }
  }
}

Bytes serialize_program(const std::string& program_id) {
  return ProgramManager::instance().serialize_program(program_id);
}

void create_program_from_bytes(const std::string& program_id, const Bytes& bytes) {
  ProgramManager::instance().create_program_from_bytes(program_id, bytes);
}

std::string validate_program(const std::string& json_program) {
  nlohmann::json_schema::json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);

  try {
    validator.set_root_schema(rtbot_schema);
  } catch (const std::exception& e) {
    return json({{"valid", false}, {"error", e.what()}}).dump();
  }

  try {
    validator.validate(json::parse(json_program));
  } catch (const std::exception& e) {
    return json({{"valid", false}, {"error", e.what()}}).dump();
  }

  return json({{"valid", true}}).dump();
}

std::string validate_operator(const std::string& type, const std::string& json_op) {
  nlohmann::json_schema::json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);

  // This is a special case
  if (type == "Pipeline") {
    try {
      auto j = json::parse(json_op);

      // Validate required fields
      std::vector<std::string> required = {"id",          "input_port_types", "output_port_types", "operators",
                                           "connections", "entryOperator",    "outputMappings"};

      for (const auto& field : required) {
        if (!j.contains(field)) {
          return R"({"valid":false,"error":"Missing required field: )" + field + "\"}";
        }
      }

      // Validate port types
      for (const auto& port_type : j["input_port_types"]) {
        std::string type = port_type.get<std::string>();
        if (type != "number" && type != "boolean" && type != "vector_number" && type != "vector_boolean") {
          return R"({"valid":false,"error":"Invalid input port type: )" + type + "\"}";
        }
      }

      for (const auto& port_type : j["output_port_types"]) {
        std::string type = port_type.get<std::string>();
        if (type != "number" && type != "boolean" && type != "vector_number" && type != "vector_boolean") {
          return R"({"valid":false,"error":"Invalid output port type: )" + type + "\"}";
        }
      }

      // Validate internal operators
      for (const auto& op : j["operators"]) {
        if (!op.contains("id") || !op.contains("type")) {
          return R"({"valid":false,"error":"Pipeline operators must have id and type fields"})";
        }

        // Recursively validate each operator
        auto validation = validate_operator(op["type"], op.dump());
        auto validation_result = json::parse(validation);
        if (!validation_result["valid"]) {
          return validation;
        }
      }

      // Validate connections
      for (const auto& conn : j["connections"]) {
        if (!conn.contains("from") || !conn.contains("to")) {
          return R"({"valid":false,"error":"Pipeline connections must specify from and to operators"})";
        }
      }

      return R"({"valid":true})";
    } catch (const json::exception& e) {
      return R"({"valid":false,"error":"Invalid JSON: )" + std::string(e.what()) + "\"}";
    }
  }

  try {
    std::optional<json> schema;
    for (const auto& it : rtbot_schema["properties"]["operators"]["items"]["oneOf"]) {
      // If is a prototype, skip
      if (it.contains("properties") && it["properties"].contains("prototype")) {
        continue;
      }

      if (type == it["properties"]["type"]["enum"][0]) {
        schema = std::optional{it};
        break;
      }
    }

    if (!schema) {
      return json({{"valid", false}, {"error", "Unknown operator type: " + type}}).dump();
    }

    validator.set_root_schema(schema.value());
  } catch (const std::exception& e) {
    return json({{"valid", false}, {"error", e.what()}}).dump();
  }

  try {
    validator.validate(json::parse(json_op));
  } catch (const std::exception& e) {
    return json({{"valid", false}, {"error", e.what()}}).dump();
  }

  return json({{"valid", true}}).dump();
}

std::string create_program(const std::string& program_id, const std::string& json_program) {
  std::string validation = validate_program(json_program);
  if (!json::parse(validation)["valid"]) {
    return validation;
  }
  return ProgramManager::instance().create_program(program_id, json_program);
}

std::string delete_program(const std::string& program_id) {
  return std::to_string(ProgramManager::instance().delete_program(program_id));
}

std::string add_to_message_buffer(const std::string& program_id, const std::string& port_id, uint64_t time,
                                  double value) {
  return std::to_string(ProgramManager::instance().add_to_message_buffer(program_id, port_id,
                                                                         Message<NumberData>(time, NumberData{value})));
}

std::string process_message_buffer(const std::string& program_id) {
  auto result = ProgramManager::instance().process_message_buffer(program_id);
  return json(result).dump();
}

std::string process_message_buffer_debug(const std::string& program_id) {
  auto result = ProgramManager::instance().process_message_buffer_debug(program_id);
  return json(result).dump();
}

std::string get_program_entry_operator_id(const std::string& program_id) {
  return ProgramManager::instance().get_program_entry_operator_id(program_id);
}

void add_batch_to_message_buffers(const std::string& program_id, const std::vector<uint64_t>& times,
                                  const std::vector<double>& values, const std::vector<std::string>& ports) {
  if (times.size() != values.size() || times.size() != ports.size()) {
    throw std::runtime_error("Vectors passed to process_batch have different lengths");
  }

  for (size_t i = 0; i < times.size(); i++) {
    ProgramManager::instance().add_to_message_buffer(program_id, ports[i],
                                                     Message<NumberData>(times[i], NumberData{values[i]}));
  }
}

std::string process_batch(const std::string& program_id, const std::vector<uint64_t>& times,
                          const std::vector<double>& values, const std::vector<std::string>& ports) {
  add_batch_to_message_buffers(program_id, times, values, ports);
  auto result = ProgramManager::instance().process_message_buffer(program_id);
  return json(result).dump();
}

std::string process_batch_debug(const std::string& program_id, const std::vector<uint64_t>& times,
                                const std::vector<double>& values, const std::vector<std::string>& ports) {
  add_batch_to_message_buffers(program_id, times, values, ports);
  auto result = ProgramManager::instance().process_message_buffer_debug(program_id);
  return json(result).dump();
}

std::string pretty_print(const std::string& json_output) {
  const std::string BLUE = "\033[0;34m";
  const std::string CYAN = "\033[0;36m";
  const std::string YELLOW = "\033[0;33m";
  const std::string RESET = "\033[0m";

  auto output = json::parse(json_output);
  std::stringstream ss;

  for (auto op_it = output.begin(); op_it != output.end(); ++op_it) {
    const auto& op_id = op_it.key();
    const auto& op_data = op_it.value();

    bool first_port = true;
    for (auto port_it = op_data.begin(); port_it != op_data.end(); ++port_it) {
      const auto& port_name = port_it.key();
      const auto& messages = port_it.value();

      if (first_port) {
        ss << BLUE << op_id << RESET << ":" << CYAN << port_name << RESET << " -> ";
        first_port = false;
      } else {
        ss << std::string(op_id.length(), ' ') << ":" << CYAN << port_name << RESET << " -> ";
      }

      bool first_msg = true;
      for (const auto& msg : messages) {
        if (!first_msg) ss << ", ";
        ss << "(" << msg["time"].get<uint64_t>() << ", " << YELLOW << msg["value"].get<double>() << RESET << ")";
        first_msg = false;
      }

      if (std::next(port_it) != op_data.end()) {
        ss << "\n";
      }
    }

    if (std::next(op_it) != output.end()) {
      ss << "\n";
    }
  }

  return ss.str();
}

std::string pretty_print(const ProgramMsgBatch& batch) {
  const std::string BLUE = "\033[0;34m";
  const std::string CYAN = "\033[0;36m";
  const std::string YELLOW = "\033[0;33m";
  const std::string RESET = "\033[0m";

  std::stringstream ss;

  for (auto op_it = batch.begin(); op_it != batch.end(); ++op_it) {
    const auto& op_id = op_it->first;
    const auto& op_batch = op_it->second;

    bool first_port = true;
    for (auto port_it = op_batch.begin(); port_it != op_batch.end(); ++port_it) {
      const auto& port_name = port_it->first;
      const auto& messages = port_it->second;

      if (first_port) {
        ss << BLUE << op_id << RESET << ":" << CYAN << port_name << RESET << " -> ";
        first_port = false;
      } else {
        ss << std::string(op_id.length(), ' ') << ":" << CYAN << port_name << RESET << " -> ";
      }

      bool first_msg = true;
      for (const auto& msg : messages) {
        if (auto* num_msg = dynamic_cast<const Message<NumberData>*>(msg.get())) {
          if (!first_msg) ss << ", ";
          ss << "(" << num_msg->time << ", " << YELLOW << num_msg->data.value << RESET << ")";
          first_msg = false;
        }
      }

      if (std::next(port_it) != op_batch.end()) {
        ss << "\n";
      }
    }

    if (std::next(op_it) != batch.end()) {
      ss << "\n";
    }
  }

  return ss.str();
}

std::string pretty_print_validation_error(const std::string& validation_result) {
  try {
    auto j = nlohmann::json::parse(validation_result);
    std::ostringstream ss;

    if (!j["valid"].get<bool>()) {
      ss << "❌ Validation Failed\n";

      if (j.contains("error")) {
        ss << "Error: " << j["error"].get<std::string>() << "\n";
      }

      if (j.contains("details")) {
        ss << "\nDetails:\n";
        const auto& details = j["details"];

        if (details.is_array()) {
          for (const auto& detail : details) {
            ss << "  • " << detail.get<std::string>() << "\n";
          }
        } else {
          ss << "  • " << details.get<std::string>() << "\n";
        }
      }

      if (j.contains("operator_errors")) {
        ss << "\nOperator Errors:\n";
        for (const auto& [op_id, error] : j["operator_errors"].items()) {
          ss << "  " << op_id << ": " << error.get<std::string>() << "\n";
        }
      }

      if (j.contains("connection_errors")) {
        ss << "\nConnection Errors:\n";
        for (const auto& error : j["connection_errors"]) {
          ss << "  • From: " << error["from"].get<std::string>() << " To: " << error["to"].get<std::string>();
          if (error.contains("message")) {
            ss << " - " << error["message"].get<std::string>();
          }
          ss << "\n";
        }
      }
    } else {
      ss << "✓ Validation Passed\n";
    }

    return ss.str();
  } catch (const nlohmann::json::exception& e) {
    return "Error parsing validation result: " + std::string(e.what());
  }
}

}  // namespace rtbot

#endif  // RTBOT_BINDINGS_H