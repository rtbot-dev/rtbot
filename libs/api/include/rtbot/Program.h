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

#include "OperatorJson.h"
#include "Prototype.h"
#include "rtbot/Logger.h"
#include "rtbot/OperatorJson.h"
#include "rtbot/jsonschema.hpp"

namespace rtbot {

using json = nlohmann::json;

using namespace std;

static void build_operator_tree(const std::map<std::string, std::shared_ptr<Operator>>& ops,
                                std::vector<std::shared_ptr<Operator>>& tree) {
  for (const auto& [_, op] : ops) {
    tree.push_back(op);

    if (auto pipeline = std::dynamic_pointer_cast<Pipeline>(op)) {
      build_operator_tree(pipeline->get_operators(), tree);
    }
  }
}

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

  // Constructor from serialized bytes
  explicit Program(const Bytes& bytes) {
    auto it = bytes.begin();

    // Deserialize program JSON
    size_t json_size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);
    program_json_ = string(it, it + json_size);
    it += json_size;

    init_from_json();

    // Read operator count
    size_t op_count = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    // Build operator tree
    std::vector<std::shared_ptr<Operator>> operator_tree;
    build_operator_tree(operators_, operator_tree);

    if (op_count != operator_tree.size()) {
      throw std::runtime_error("Operator count mismatch in restore: stored=" + std::to_string(op_count) +
                               ", actual=" + std::to_string(operator_tree.size()));
    }

    // Restore each operator's state
    for (auto& op : operator_tree) {
      op->restore(it);
    }
  }

  Bytes serialize() {
    Bytes bytes;

    // Serialize program JSON
    size_t size = program_json_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&size),
                 reinterpret_cast<const uint8_t*>(&size) + sizeof(size));
    bytes.insert(bytes.end(), program_json_.begin(), program_json_.end());

    // Build complete operator tree
    std::vector<std::shared_ptr<Operator>> operator_tree;
    build_operator_tree(operators_, operator_tree);

    // Store operator count
    size_t op_count = operator_tree.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&op_count),
                 reinterpret_cast<const uint8_t*>(&op_count) + sizeof(op_count));

    // Serialize each operator's state
    for (const auto& op : operator_tree) {
      Bytes op_state = op->collect();
      bytes.insert(bytes.end(), op_state.begin(), op_state.end());
    }

    return bytes;
  }

  // Message processing
  ProgramMsgBatch receive(const Message<NumberData>& msg, const std::string& port_id = "i1") {
    send_to_entry(msg, port_id);
    ProgramMsgBatch result = collect_outputs(false);
    clear_all_outputs();
    return result;
  }

  ProgramMsgBatch receive_debug(const Message<NumberData>& msg, const std::string& port_id = "i1") {
    send_to_entry(msg, port_id);
    ProgramMsgBatch result = collect_outputs(true);
    clear_all_outputs();
    return result;
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

    for (const json& op_json : j["operators"]) {
      string id = op_json["id"];
      RTBOT_LOG_DEBUG("Creating operator: ", op_json.dump());
      operators_[id] = OperatorJson::read_op(op_json.dump());
      RTBOT_LOG_DEBUG("...created operator: ", id);
    }

    RTBOT_LOG_DEBUG("All operators created, connecting them");

    for (const json& conn : j["connections"]) {
      RTBOT_LOG_DEBUG("Connecting operators: ", conn.dump());
      string from_id = conn["from"];
      string to_id = conn["to"];

      // Default to "o1" and "i1" if ports not specified
      auto from_port = OperatorJson::parse_port_name(conn.value("fromPort", "o1"));
      auto to_port = OperatorJson::parse_port_name(conn.value("toPort", "i1"));

      if (conn.contains("toPortType")) {
        to_port.kind = conn["toPortType"] == "control" ? PortKind::CONTROL : PortKind::DATA;
      }

      if (!operators_[from_id] || !operators_[to_id]) {
        throw runtime_error("Program: invalid operator reference in connection from " + from_id + " to " + to_id);
      }

      operators_[from_id]->connect(operators_[to_id], from_port.index, to_port.index, to_port.kind);
    }

    entry_operator_id_ = j["entryOperator"];
    RTBOT_LOG_DEBUG("Entry operator: ", entry_operator_id_);
    if (!operators_[entry_operator_id_]) {
      throw runtime_error("Entry operator not found: " + entry_operator_id_);
    }

    // Parse output mappings
    if (!j.contains("output")) {
      throw runtime_error("Program JSON must contain 'output' field");
    }

    for (auto it = j["output"].begin(); it != j["output"].end(); ++it) {
      string op_id = it.key();
      vector<size_t> ports;
      for (const auto& port : it.value()) {
        ports.push_back(OperatorJson::parse_port_name(port).index);
      }
      output_mappings_[op_id] = ports;
    }
    RTBOT_LOG_DEBUG("Program initialized");
  }

  void send_to_entry(const Message<NumberData>& msg, const std::string& port_id) {
    auto port_info = OperatorJson::parse_port_name(port_id);
    operators_[entry_operator_id_]->receive_data(create_message<NumberData>(msg.time, msg.data), port_info.index);
    operators_[entry_operator_id_]->execute();
  }

  ProgramMsgBatch collect_filtered_outputs() {
    ProgramMsgBatch batch;

    for (const auto& [op_id, port_indices] : output_mappings_) {
      if (operators_.find(op_id) != operators_.end()) {
        OperatorMsgBatch op_batch;

        for (size_t port_idx : port_indices) {
          string port_name = "o" + to_string(port_idx + 1);
          const auto& queue = operators_[op_id]->get_output_queue(port_idx);

          if (!queue.empty()) {
            PortMsgBatch port_msgs;
            for (const auto& msg : queue) {
              port_msgs.push_back(std::move(msg->clone()));
            }
            op_batch[port_name] = std::move(port_msgs);
          }
        }

        if (!op_batch.empty()) {
          batch[op_id] = std::move(op_batch);
        }
      }
    }

    return batch;
  }

  ProgramMsgBatch collect_outputs(bool debug_mode = false) {
    ProgramMsgBatch batch;

    std::function<void(const std::string&, const std::shared_ptr<Operator>&)> collect_operator_outputs =
        [&](const std::string& op_id, const std::shared_ptr<Operator>& op) {
          // Skip if not in debug mode and this operator is not in output_mappings_
          if (!debug_mode && output_mappings_.find(op_id) == output_mappings_.end()) {
            return;
          }

          OperatorMsgBatch op_batch;
          bool has_messages = false;

          // In debug mode, collect all ports
          if (debug_mode) {
            for (size_t i = 0; i < op->num_output_ports(); i++) {
              const auto& queue = op->get_output_queue(i);
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
            const auto& port_indices = output_mappings_[op_id];
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
            batch[op_id] = std::move(op_batch);
          }

          // Only traverse pipeline if in debug mode
          if (debug_mode) {
            if (auto pipeline = std::dynamic_pointer_cast<Pipeline>(op)) {
              for (const auto& [internal_id, internal_op] : pipeline->get_operators()) {
                collect_operator_outputs(internal_id, internal_op);
              }
            }
          }
        };

    for (const auto& [op_id, op] : operators_) {
      collect_operator_outputs(op_id, op);
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

  void create_program_from_bytes(const std::string& id, const Bytes& bytes) {
    if (programs_.count(id) > 0) {
      throw std::runtime_error("Program " + id + " already exists");
    }
    programs_.emplace(id, Program(bytes));
    return;
  }

  bool add_to_message_buffer(const string& program_id, const string& port_id, const Message<NumberData>& msg) {
    if (programs_.count(program_id) == 0) return false;
    message_buffer_[program_id][port_id].push_back(msg);
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
          auto batch = prog.receive(msg, port_id);
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
          auto batch = prog.receive_debug(msg, port_id);
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

  Bytes serialize_program(const string& program_id) { return get_program(program_id).serialize(); }

  bool delete_program(const string& program_id) { return programs_.erase(program_id) > 0; }

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

  map<string, Program> programs_;
  map<string, map<string, vector<Message<NumberData>>>> message_buffer_;
};

}  // namespace rtbot

#endif  // PROGRAM_H
