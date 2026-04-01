#ifndef PROGRAM_H
#define PROGRAM_H

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <vector>
#include <variant>

#include "OperatorJson.h"
#include "Prototype.h"
#include "rtbot/Logger.h"
#include "rtbot/OperatorJson.h"
#include "rtbot/jsonschema.hpp"

namespace rtbot {

using json = nlohmann::json;

using namespace std;

class Program {
 public:
  // Constructor from JSON string
  explicit Program(const std::string& json_string) : program_json_(json_string) {
    auto j = json::parse(program_json_);

    // Resolve prototypes before validation
    PrototypeHandler::resolve_prototypes(j);

    // Update program_json_ with resolved version
    program_json_ = j.dump();

    // Continue with existing validation and initialization
    nlohmann::json_schema::json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);
    validator.set_root_schema(rtbot_schema);
    validator.validate(j);

    init_from_json();
  }

  string serialize_data() {
    json result;
    for (auto& [name, op] : operators_) {
      result[name] = op->collect();
    }
    return result.dump();
  }

  void restore_data_from_json(const string& json_state) {
    auto j = json::parse(json_state);
    for (auto& [name, op] : operators_) {
      op->restore_data_from_json(j.at(name));
    }
  }

  // Message processing
  ProgramMsgBatch receive(std::unique_ptr<BaseMessage> msg, const std::string& port_id = "i1") {
    send_to_entry(std::move(msg), port_id, false);
    ProgramMsgBatch result = collect_outputs(false);
    clear_all_outputs();
    return result;
  }

  ProgramMsgBatch receive(const Message<NumberData>& msg, const std::string& port_id = "i1") {
    return receive(create_message<NumberData>(msg.time, msg.data), port_id);
  }

  ProgramMsgBatch receive_debug(std::unique_ptr<BaseMessage> msg, const std::string& port_id = "i1") {
    send_to_entry(std::move(msg), port_id, true);
    ProgramMsgBatch result = collect_outputs(true);
    clear_all_outputs();
    return result;
  }

  ProgramMsgBatch receive_debug(const Message<NumberData>& msg, const std::string& port_id = "i1") {
    return receive_debug(create_message<NumberData>(msg.time, msg.data), port_id);
  }

  // Getters
  const string& get_entry_operator() const { return entry_operator_id_; }
  const map<string, vector<size_t>>& get_output_mappings() const { return output_mappings_; }
  vector<string> entry_ports() const;

  // Static factory methods

 private:
  string program_json_;
  map<string, shared_ptr<Operator>> operators_;
  string entry_operator_id_;
  map<string, vector<size_t>> output_mappings_;

  void init_from_json() {
    RTBOT_LOG_DEBUG("Initializing program from JSON");
    auto j = json::parse(program_json_);

    // First pass: Create all operators
    for (const json& op_json : j["operators"]) {
      create_operator("", op_json);
    }

    // Second pass: Create connections
    for (const json& conn : j["connections"]) {
      create_connection(conn);
    }

    entry_operator_id_ = j["entryOperator"];
    if (!operators_[entry_operator_id_]) {
      throw runtime_error("Entry operator not found: " + entry_operator_id_);
    }

    // Parse output mappings
    if (!j.contains("output")) {
      throw runtime_error("Program JSON must contain 'output' field");
    }

    for (auto it = j["output"].begin(); it != j["output"].end(); ++it) {
      string op_id = resolve_operator_id(it.key());  // Use resolve_operator_id here
      vector<size_t> ports;
      for (const auto& port : it.value()) {
        ports.push_back(OperatorJson::parse_port_name(port).index);
      }
      output_mappings_[op_id] = ports;
    }
  }

  void create_operator(const std::string& parent_prefix, const json& op_json) {
    string local_id = op_json["id"];
    string qualified_id = parent_prefix.empty() ? local_id : parent_prefix + "::" + local_id;

    auto op = OperatorJson::read_op(op_json.dump());
    operators_[qualified_id] = op;

    // If this is a Pipeline, recursively create its internal operators
    if (auto pipeline = std::dynamic_pointer_cast<Pipeline>(op)) {
      auto pipeline_json = json::parse(op_json.dump());
      if (pipeline_json.contains("operators")) {
        for (const auto& internal_op : pipeline_json["operators"]) {
          create_operator(qualified_id, internal_op);
        }
      }
    }
  }

  void create_connection(const json& conn) {
    string from_qual_id = resolve_operator_id(conn["from"]);
    string to_qual_id = resolve_operator_id(conn["to"]);

    auto from_port = OperatorJson::parse_port_name(conn.value("fromPort", "o1"));
    auto to_port = OperatorJson::parse_port_name(conn.value("toPort", "i1"));

    if (conn.contains("toPortType")) {
      to_port.kind = conn["toPortType"] == "control" ? PortKind::CONTROL : PortKind::DATA;
    }

    if (!operators_[from_qual_id] || !operators_[to_qual_id]) {
      throw runtime_error("Invalid operator reference in connection from " + from_qual_id + " to " + to_qual_id);
    }

    operators_[from_qual_id]->connect(operators_[to_qual_id], from_port.index, to_port.index, to_port.kind);
  }

  string resolve_operator_id(const string& id) {
    // First check if it's already a qualified ID
    if (operators_.find(id) != operators_.end()) {
      return id;
    }

    // Search through all qualified IDs for a match
    string suffix = "::" + id;
    for (const auto& [qual_id, _] : operators_) {
      if ((qual_id.length() >= suffix.length() &&
           qual_id.compare(qual_id.length() - suffix.length(), suffix.length(), suffix) == 0) ||
          qual_id == id) {
        return qual_id;
      }
    }

    throw runtime_error("Could not resolve operator ID: " + id);
  }

  void send_to_entry(std::unique_ptr<BaseMessage> msg, const std::string& port_id, bool debug = false) {
    auto port_info = OperatorJson::parse_port_name(port_id);
    operators_[entry_operator_id_]->receive_data(std::move(msg), port_info.index);
    operators_[entry_operator_id_]->execute(debug);
  }

  ProgramMsgBatch collect_outputs(bool debug_mode = false) {
    ProgramMsgBatch batch;

    std::function<void(const std::string&, const std::shared_ptr<Operator>&, const std::string&)>
        collect_operator_outputs =
            [&](const std::string& op_id, const std::shared_ptr<Operator>& op, const std::string& parent_prefix) {
              // Build fully qualified ID
              std::string qualified_id = parent_prefix.empty() ? op_id : parent_prefix + "::" + op_id;

              // Skip if not in debug mode and this operator is not in output_mappings_
              if (!debug_mode && output_mappings_.find(qualified_id) == output_mappings_.end()) {
                return;
              }

              OperatorMsgBatch op_batch;
              bool has_messages = false;

              // In debug mode, collect all ports
              if (debug_mode) {
                for (size_t i = 0; i < op->num_output_ports(); i++) {
                  const auto& queue = op->get_debug_output_queue(i);
                  if (!queue.empty()) {
                    PortMsgBatch port_msgs;
                    for (const auto& msg : queue) {
                      port_msgs.push_back(msg->clone());
                    }
                    op_batch["o" + std::to_string(i + 1)] = std::move(port_msgs);
                    has_messages = true;
                  }
                }
              }
              // Otherwise only collect mapped ports
              else {
                const auto& port_indices = output_mappings_[qualified_id];
                for (size_t port_idx : port_indices) {
                  const auto& queue = op->get_output_queue(port_idx);
                  if (!queue.empty()) {
                    PortMsgBatch port_msgs;
                    for (const auto& msg : queue) {
                      port_msgs.push_back(msg->clone());
                    }
                    op_batch["o" + std::to_string(port_idx + 1)] = std::move(port_msgs);
                    has_messages = true;
                  }
                }
              }

              if (has_messages) {
                batch[qualified_id] = std::move(op_batch);
              }

              // Recursively handle Pipeline operators
              if (auto pipeline = std::dynamic_pointer_cast<Pipeline>(op)) {
                for (const auto& [internal_id, internal_op] : pipeline->get_operators()) {
                  // Pass qualified_id as the new parent prefix for nested operators
                  collect_operator_outputs(internal_id, internal_op, qualified_id);
                }

                // Handle Pipeline output mappings in debug mode
                if (debug_mode && pipeline->get_output_mappings().size() > 0) {
                  for (const auto& [mapped_op_id, mappings] : pipeline->get_output_mappings()) {
                    auto mapped_qualified_id = qualified_id + "::" + mapped_op_id;
                    auto mapped_op = pipeline->get_operators().find(mapped_op_id);
                    if (mapped_op != pipeline->get_operators().end()) {
                      for (const auto& [op_port, pipeline_port] : mappings) {
                        const auto& queue = mapped_op->second->get_output_queue(op_port);
                        if (!queue.empty()) {
                          if (batch[mapped_qualified_id].empty()) {
                            batch[mapped_qualified_id] = OperatorMsgBatch();
                          }
                          PortMsgBatch& port_msgs = batch[mapped_qualified_id]["o" + std::to_string(pipeline_port + 1)];
                          for (const auto& msg : queue) {
                            port_msgs.push_back(msg->clone());
                          }
                        }
                      }
                    }
                  }
                }
              }
            };

    // Start collection from top-level operators with empty parent prefix
    for (const auto& [op_id, op] : operators_) {
      collect_operator_outputs(op_id, op, "");
    }

    return batch;
  }

  void clear_all_outputs() {
    for (auto& [_, op] : operators_) {
      op->clear_all_output_ports();
    }
  }
};

// Program Manager class to handle multiple programs
class ProgramManager {
 public:
  static ProgramManager& instance() {
    static ProgramManager instance;
    return instance;
  }

  void clear_all_programs() {
    programs_.clear();
    message_buffer_.clear();
    vector_builders_.clear();
  }

  string create_program(const string& id, const string& json_program) {
    if (programs_.count(id) > 0) {
      std::cout << "Program " << id << " already exists, count = " << programs_.count(id) << std::endl;
      return "Program " + id + " already exists";
    }
    try {
      programs_.emplace(id, Program(json_program));
      return "";
    } catch (const exception& e) {
      return string("Failed to create program: ") + e.what();
    }
  }

  bool add_to_message_buffer(const string& program_id, const string& port_id, const Message<NumberData>& msg,
                             uint64_t id = 0) {
    if (programs_.count(program_id) == 0) return false;
    message_buffer_[program_id][port_id].push_back(create_message<NumberData>(msg.time, id, msg.data));
    return true;
  }

  bool begin_vector_message(const string& program_id, const string& port_id, timestamp_t time,
                            uint64_t id = 0) {
    if (programs_.count(program_id) == 0) return false;
    auto& builder = vector_builders_[program_id][port_id];
    if (builder.active) return false;
    builder.active = true;
    builder.time = time;
    builder.id = id;
    builder.values.clear();
    return true;
  }

  bool push_vector_message_value(const string& program_id, const string& port_id, double value) {
    auto prog_it = vector_builders_.find(program_id);
    if (prog_it == vector_builders_.end()) return false;
    auto port_it = prog_it->second.find(port_id);
    if (port_it == prog_it->second.end() || !port_it->second.active) return false;
    port_it->second.values.push_back(value);
    return true;
  }

  bool end_vector_message(const string& program_id, const string& port_id) {
    if (programs_.count(program_id) == 0) return false;
    auto prog_it = vector_builders_.find(program_id);
    if (prog_it == vector_builders_.end()) return false;
    auto port_it = prog_it->second.find(port_id);
    if (port_it == prog_it->second.end() || !port_it->second.active) return false;

    auto& builder = port_it->second;
    message_buffer_[program_id][port_id].push_back(
        create_message<VectorNumberData>(builder.time, builder.id, VectorNumberData{builder.values}));
    builder.active = false;
    builder.values.clear();
    return true;
  }

  bool abort_vector_message(const string& program_id, const string& port_id) {
    auto prog_it = vector_builders_.find(program_id);
    if (prog_it == vector_builders_.end()) return false;
    auto port_it = prog_it->second.find(port_id);
    if (port_it == prog_it->second.end() || !port_it->second.active) return false;
    port_it->second.active = false;
    port_it->second.values.clear();
    return true;
  }

  ProgramMsgBatch process_message_buffer(const string& program_id) {
    if (programs_.count(program_id) == 0) {
      throw runtime_error("Program " + program_id + " not found");
    }

    ProgramMsgBatch result;
    auto& prog = programs_.at(program_id);

    auto buffer_it = message_buffer_.find(program_id);
    if (buffer_it != message_buffer_.end() && !buffer_it->second.empty()) {
      for (const auto& [port_id, messages] : buffer_it->second) {
        for (const auto& msg : messages) {
          auto batch = prog.receive(msg->clone(), port_id);
          merge_batches(result, batch);
        }
      }
      message_buffer_.erase(buffer_it);
    }
    return result;
  }

  ProgramMsgBatch process_message_buffer_debug(const string& program_id) {
    if (programs_.count(program_id) == 0) {
      throw runtime_error("Program " + program_id + " not found");
    }

    ProgramMsgBatch result;
    auto& prog = programs_.at(program_id);

    auto buffer_it = message_buffer_.find(program_id);
    if (buffer_it != message_buffer_.end() && !buffer_it->second.empty()) {
      for (const auto& [port_id, messages] : buffer_it->second) {
        for (const auto& msg : messages) {
          auto batch = prog.receive_debug(msg->clone(), port_id);
          merge_batches(result, batch);
        }
      }
      message_buffer_.erase(buffer_it);
    }
    return result;
  }

  string get_program_entry_operator_id(const string& program_id) {
    try {
      return get_program(program_id).get_entry_operator();
    } catch (const exception&) {
      return "";
    }
  }

  string serialize_program_data(const string& program_id) { return get_program(program_id).serialize_data(); }

  void restore_program_data_from_json(const string& program_id, const string& json_state) {
    get_program(program_id).restore_data_from_json(json_state);
  }

  bool delete_program(const string& program_id) {
    vector_builders_.erase(program_id);
    return programs_.erase(program_id) > 0;
  }

 private:
  Program& get_program(const string& program_id) {
    auto it = programs_.find(program_id);
    if (it == programs_.end()) {
      throw runtime_error("Program " + program_id + " not found");
    }
    return it->second;
  }

  void merge_batches(ProgramMsgBatch& target, const ProgramMsgBatch& source) {
    for (const auto& [op_id, op_batch] : source) {
      for (const auto& [port_name, port_msgs] : op_batch) {
        auto& target_port = target[op_id][port_name];
        target_port.reserve(target_port.size() + port_msgs.size());
        for (const auto& msg : port_msgs) {
          target_port.push_back(std::move(msg->clone()));
        }
      }
    }
  }

  struct VectorBuilderState {
    bool active = false;
    timestamp_t time = 0;
    uint64_t id = 0;
    std::vector<double> values;
  };

  map<string, Program> programs_;
  map<string, map<string, vector<std::unique_ptr<BaseMessage>>>> message_buffer_;
  map<string, map<string, VectorBuilderState>> vector_builders_;
};

}  // namespace rtbot

#endif  // PROGRAM_H
