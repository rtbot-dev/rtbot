#ifndef PROGRAM_H
#define PROGRAM_H

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "rtbot/OperatorJson.h"

namespace rtbot {

using json = nlohmann::json;

using namespace std;

class Program {
 public:
  // Constructor from JSON string
  explicit Program(const string& json_string) : program_json_(json_string) { init_from_json(); }

  // Constructor from serialized bytes
  explicit Program(const Bytes& bytes) {
    auto it = bytes.begin();
    size_t json_size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    program_json_ = string(it, it + json_size);
    it += json_size;

    init_from_json();

    while (it < bytes.end()) {
      size_t id_size = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);
      string id(it, it + id_size);
      it += id_size;

      auto op_it = operators_.find(id);
      if (op_it != operators_.end()) {
        op_it->second->restore(it);
      }
    }
  }

  // Serialization
  Bytes serialize() {
    Bytes bytes;

    size_t size = program_json_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&size),
                 reinterpret_cast<const uint8_t*>(&size) + sizeof(size));
    bytes.insert(bytes.end(), program_json_.begin(), program_json_.end());

    for (const auto& [id, op] : operators_) {
      size = id.size();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&size),
                   reinterpret_cast<const uint8_t*>(&size) + sizeof(size));
      bytes.insert(bytes.end(), id.begin(), id.end());

      Bytes op_state = op->collect();
      bytes.insert(bytes.end(), op_state.begin(), op_state.end());
    }

    return bytes;
  }

  // Message processing
  ProgramMsgBatch receive(const Message<NumberData>& msg) {
    send_to_entry(msg);
    ProgramMsgBatch result = collect_filtered_outputs();
    clear_all_outputs();
    return result;
  }

  ProgramMsgBatch receive_debug(const Message<NumberData>& msg) {
    send_to_entry(msg);
    ProgramMsgBatch result = collect_all_outputs();
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
    auto j = json::parse(program_json_);

    for (const json& op_json : j["operators"]) {
      string id = op_json["id"];
      operators_[id] = OperatorJson::read_op(op_json.dump());
    }

    for (const json& conn : j["connections"]) {
      string from_id = conn["from"];
      string to_id = conn["to"];

      // Default to "o1" and "i1" if ports not specified
      size_t from_port = conn.contains("fromPort") ? port_name_to_index(conn["fromPort"]) : 0;
      size_t to_port = conn.contains("toPort") ? port_name_to_index(conn["toPort"]) : 0;

      if (!operators_[from_id] || !operators_[to_id]) {
        throw runtime_error("Invalid operator reference in connection");
      }

      operators_[from_id]->connect(operators_[to_id], from_port, to_port);
    }

    entry_operator_id_ = j["entryOperator"];
    if (!operators_[entry_operator_id_]) {
      throw runtime_error("Entry operator not found: " + entry_operator_id_);
    }

    for (const json& mapping : j["outputs"]) {
      string op_id = mapping["operatorId"];
      vector<size_t> ports;
      for (const auto& port : mapping["ports"]) {
        ports.push_back(port_name_to_index(port));
      }
      output_mappings_[op_id] = ports;
    }
  }

  size_t port_name_to_index(const string& port_name) {
    if (port_name.empty()) return 0;

    if (port_name[0] == 'i' || port_name[0] == 'o') {
      try {
        return stoul(port_name.substr(1)) - 1;
      } catch (...) {
        throw runtime_error("Invalid port name format: " + port_name);
      }
    }

    try {
      return stoul(port_name);
    } catch (...) {
      throw runtime_error("Invalid port specification: " + port_name);
    }
  }

  void send_to_entry(const Message<NumberData>& msg) {
    operators_[entry_operator_id_]->receive_data(create_message<NumberData>(msg.time, msg.data), 0);
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
              port_msgs.push_back(msg->clone());
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

  ProgramMsgBatch collect_all_outputs() {
    ProgramMsgBatch batch;

    for (const auto& [op_id, op] : operators_) {
      OperatorMsgBatch op_batch;
      bool has_messages = false;

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

      if (has_messages) {
        batch[op_id] = std::move(op_batch);
      }
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
          auto batch = prog.receive(msg);
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
          auto batch = prog.receive_debug(msg);
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
          target_port.push_back(msg->clone());
        }
      }
    }
  }

  // Rest of the class implementation remains the same
  map<string, Program> programs_;
  map<string, map<string, vector<Message<NumberData>>>> message_buffer_;
};

}  // namespace rtbot

#endif  // PROGRAM_H