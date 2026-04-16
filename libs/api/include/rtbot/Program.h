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
#include "rtbot/Collector.h"
#include "rtbot/Logger.h"
#include "rtbot/OperatorJson.h"
#include "rtbot/PortType.h"
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
    return collect_outputs(false);
  }

  ProgramMsgBatch receive(const Message<NumberData>& msg, const std::string& port_id = "i1") {
    return receive(create_message<NumberData>(msg.time, msg.data), port_id);
  }

  ProgramMsgBatch receive_debug(std::unique_ptr<BaseMessage> msg, const std::string& port_id = "i1") {
    for (auto& [_, op] : operators_) {
      op->clear_debug_output_queues();
    }
    send_to_entry(std::move(msg), port_id, true);
    return collect_outputs(true);
  }

  ProgramMsgBatch receive_debug(const Message<NumberData>& msg, const std::string& port_id = "i1") {
    return receive_debug(create_message<NumberData>(msg.time, msg.data), port_id);
  }

  // Batch entry: push all messages from a multi-port buffer into the entry
  // operator's queues, then run a single execute() pass, then collect+clear
  // outputs once. Semantically equivalent to calling receive() once per
  // message in arrival order, but amortizes the per-message scheduling cost
  // (virtual dispatch, propagate_outputs recursion, collect/clear) over the
  // whole burst. Relies on the rtbot invariant that messages within a port
  // are monotone in time; sync_data_inputs is state-preserving regardless of
  // how many messages are queued on each port.
  ProgramMsgBatch receive_batch(
      const std::map<std::string, std::vector<std::unique_ptr<BaseMessage>>>& port_messages) {
    send_batch_to_entry(port_messages, false);
    return collect_outputs(false);
  }

  ProgramMsgBatch receive_batch_debug(
      const std::map<std::string, std::vector<std::unique_ptr<BaseMessage>>>& port_messages) {
    for (auto& [_, op] : operators_) {
      op->clear_debug_output_queues();
    }
    send_batch_to_entry(port_messages, true);
    return collect_outputs(true);
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
  // Program-owned multi-port sink. One Collector with one data port per
  // output in output_mappings_; emit_output pushes straight into the
  // matching port queue via the is_sink() fast path (no virtual
  // receive_data). Provenance (source operator + output port) is read from
  // each port's inbound connection ref — no separate index needed.
  std::shared_ptr<Collector> sink_;

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

    // Attach Program-owned sinks to output-mapped operators.
    setup_output_sinks_();
  }

  void setup_output_sinks_() {
    struct OutputBinding { std::shared_ptr<Operator> op; size_t port_idx; };
    std::vector<OutputBinding> bindings;
    std::vector<std::string> port_types;

    for (const auto& [op_id, ports] : output_mappings_) {
      auto op_it = operators_.find(op_id);
      if (op_it == operators_.end()) continue;
      for (size_t port_idx : ports) {
        port_types.push_back(
            PortType::type_index_to_string(op_it->second->get_output_port_type(port_idx)));
        bindings.push_back({op_it->second, port_idx});
      }
    }

    if (bindings.empty()) return;

    sink_ = make_collector("__program_sink__", port_types);
    for (size_t i = 0; i < bindings.size(); ++i) {
      bindings[i].op->connect(sink_, bindings[i].port_idx, /*child_port_index=*/i, PortKind::DATA);
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

    // If this is a TriggerSet, recursively create its internal operators
    if (auto trigger_set = std::dynamic_pointer_cast<TriggerSet>(op)) {
      auto ts_json = json::parse(op_json.dump());
      if (ts_json.contains("operators")) {
        for (const auto& internal_op : ts_json["operators"]) {
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

  void send_batch_to_entry(
      const std::map<std::string, std::vector<std::unique_ptr<BaseMessage>>>& port_messages, bool debug) {
    auto& entry = operators_[entry_operator_id_];
    for (const auto& [port_id, messages] : port_messages) {
      if (messages.empty()) continue;
      auto port_info = OperatorJson::parse_port_name(port_id);
      for (const auto& msg : messages) {
        entry->receive_data(msg->clone(), port_info.index);
      }
    }
    entry->execute(debug);
  }

  ProgramMsgBatch collect_outputs(bool debug_mode = false) {
    ProgramMsgBatch batch;

    if (!debug_mode) {
      // Non-debug fast path: drain the single multi-port sink and read
      // provenance (upstream operator id + output port) from each port's
      // inbound connection ref.
      if (!sink_) return batch;

      const size_t n = sink_->num_data_ports();
      for (size_t p = 0; p < n; ++p) {
        auto& q = sink_->get_data_queue(p);
        if (q.empty()) continue;

        // Port p was wired by setup_output_sinks_() to exactly one upstream;
        // invariant enforced by the only writer of sink_.
        const auto& ref = sink_->inbound_data_refs(p).front();
        const auto& conn = ref.parent->get_connection(ref.conn_index);

        PortMsgBatch port_msgs;
        port_msgs.reserve(q.size());
        for (auto& msg : q) port_msgs.push_back(std::move(msg));
        q.clear();

        batch[ref.parent->id()]["o" + std::to_string(conn.output_port + 1)] =
            std::move(port_msgs);
      }
      return batch;
    }

    // Debug mode: walk every operator (including composite children) and
    // collect from debug output queues.
    std::function<void(const std::string&, const std::shared_ptr<Operator>&, const std::string&)>
        collect_operator_outputs =
            [&](const std::string& op_id, const std::shared_ptr<Operator>& op, const std::string& parent_prefix) {
              std::string qualified_id = parent_prefix.empty() ? op_id : parent_prefix + "::" + op_id;

              OperatorMsgBatch op_batch;
              bool has_messages = false;

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

              if (has_messages) {
                batch[qualified_id] = std::move(op_batch);
              }

              // Recursively handle composite operators (Pipeline, TriggerSet).
              if (const auto* kids = op->children_ops()) {
                for (const auto& [internal_id, internal_op] : *kids) {
                  collect_operator_outputs(internal_id, internal_op, qualified_id);
                }
              }
            };

    for (const auto& [op_id, op] : operators_) {
      collect_operator_outputs(op_id, op, "");
    }

    return batch;
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

  bool add_to_message_buffer(const string& program_id, const string& port_id, const Message<NumberData>& msg) {
    if (programs_.count(program_id) == 0) return false;
    message_buffer_[program_id][port_id].push_back(create_message<NumberData>(msg.time, msg.data));
    return true;
  }

  bool begin_vector_message(const string& program_id, const string& port_id, timestamp_t time) {
    if (programs_.count(program_id) == 0) return false;
    auto& builder = vector_builders_[program_id][port_id];
    if (builder.active) return false;
    builder.active = true;
    builder.time = time;
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
        create_message<VectorNumberData>(builder.time, VectorNumberData{builder.values}));
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

    auto& prog = programs_.at(program_id);
    auto buffer_it = message_buffer_.find(program_id);
    if (buffer_it == message_buffer_.end() || buffer_it->second.empty()) {
      return {};
    }

    ProgramMsgBatch result = prog.receive_batch(buffer_it->second);
    message_buffer_.erase(buffer_it);
    return result;
  }

  ProgramMsgBatch process_message_buffer_debug(const string& program_id) {
    if (programs_.count(program_id) == 0) {
      throw runtime_error("Program " + program_id + " not found");
    }

    auto& prog = programs_.at(program_id);
    auto buffer_it = message_buffer_.find(program_id);
    if (buffer_it == message_buffer_.end() || buffer_it->second.empty()) {
      return {};
    }

    ProgramMsgBatch result = prog.receive_batch_debug(buffer_it->second);
    message_buffer_.erase(buffer_it);
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
    std::vector<double> values;
  };

  map<string, Program> programs_;
  map<string, map<string, vector<std::unique_ptr<BaseMessage>>>> message_buffer_;
  map<string, map<string, VectorBuilderState>> vector_builders_;
};

}  // namespace rtbot

#endif  // PROGRAM_H
