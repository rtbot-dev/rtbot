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

  try {
    std::optional<json> schema;
    for (const auto& it : rtbot_schema["properties"]["operators"]["items"]["oneOf"]) {
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
  const std::string RESET = "\033[0m";
  const std::string BOLD = "\033[1m";
  const std::string RED = "\033[31m";
  const std::string GREEN = "\033[32m";
  const std::string YELLOW = "\033[33m";
  const std::string BLUE = "\033[34m";
  const std::string MAGENTA = "\033[35m";

  json output = json::parse(json_output);
  std::stringstream ss;

  ss << BOLD << BLUE << "=== Program Output ===" << RESET << "\n\n";

  for (const auto& [op_id, op_data] : output.items()) {
    ss << BOLD << MAGENTA << "Operator: " << op_id << RESET << "\n";

    for (const auto& [port_name, messages] : op_data.items()) {
      ss << YELLOW << "  Port: " << port_name << RESET << "\n";

      for (const auto& msg : messages) {
        ss << GREEN << "    Time: " << std::setw(10) << msg["time"].get<uint64_t>();
        ss << RED << " | Value: " << std::setw(10) << std::fixed << std::setprecision(4) << msg["value"].get<double>()
           << RESET << "\n";
      }
      ss << "\n";
    }
  }

  return ss.str();
}

std::string pretty_print(const ProgramMsgBatch& batch) {
  const std::string RESET = "\033[0m";
  const std::string BOLD = "\033[1m";
  const std::string RED = "\033[31m";
  const std::string GREEN = "\033[32m";
  const std::string YELLOW = "\033[33m";
  const std::string BLUE = "\033[34m";
  const std::string MAGENTA = "\033[35m";

  std::stringstream ss;
  ss << BOLD << BLUE << "=== Program Output ===" << RESET << "\n\n";

  for (const auto& [op_id, op_batch] : batch) {
    ss << BOLD << MAGENTA << "Operator: " << op_id << RESET << "\n";

    for (const auto& [port_name, messages] : op_batch) {
      ss << YELLOW << "  Port: " << port_name << RESET << "\n";

      for (const auto& msg : messages) {
        if (auto* num_msg = dynamic_cast<const Message<NumberData>*>(msg.get())) {
          ss << GREEN << "    Time: " << std::setw(10) << num_msg->time;
          ss << RED << " | Value: " << std::setw(10) << std::fixed << std::setprecision(4) << num_msg->data.value
             << RESET << "\n";
        }
      }
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