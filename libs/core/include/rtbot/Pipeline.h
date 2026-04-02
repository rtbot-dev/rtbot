#ifndef PIPELINE_H
#define PIPELINE_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Logger.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Pipeline : public Operator {
 public:
  struct PipelineConnection {
    std::string from_id;
    std::string to_id;
    size_t from_port{0};
    size_t to_port{0};
  };

  Pipeline(std::string id, const std::vector<std::string>& input_port_types,
           const std::vector<std::string>& output_port_types)
      : Operator(std::move(id)) {
    // Configure input ports
    for (const auto& type : input_port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown input port type: " + type);
      }
      PortType::add_port(*this, type, true, false, false);
      input_port_types_.push_back(type);
    }

    // Configure output ports
    for (const auto& type : output_port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown output port type: " + type);
      }
      PortType::add_port(*this, type, false, false, true);
      output_port_types_.push_back(type);
    }

    // Control port: receives segment expression value (NumberData)
    // Key transitions (value changes) drive output emission and reset
    add_control_port<NumberData>();
  }

  // Get port configurations
  const std::vector<std::string>& get_input_port_types() const { return input_port_types_; }
  const std::vector<std::string>& get_output_port_types() const { return output_port_types_; }
  const std::map<std::string, std::shared_ptr<Operator>>& get_operators() const { return operators_; }
  const std::vector<PipelineConnection>& get_pipeline_connections() const { return pipeline_connections_; }
  const std::string& get_entry_operator_id() const { return entry_operator_->id(); }
  const std::map<std::string, std::vector<std::pair<size_t, size_t>>>& get_output_mappings() const {
    return output_mappings_;
  }

  // API for configuring the pipeline
  void register_operator(std::shared_ptr<Operator> op) { operators_[op->id()] = std::move(op); }

  void set_entry(const std::string& op_id) {
    auto it = operators_.find(op_id);
    if (it == operators_.end()) {
      throw std::runtime_error("Entry operator not found: " + op_id);
    }
    if (it->second->num_data_ports() >= num_data_ports()) {
      entry_operator_ = it->second;
      RTBOT_LOG_DEBUG("Setting entry operator: ", op_id);
    } else {
      throw std::runtime_error("Entry operator has less data ports that the pipeline: " + op_id);
    }
  }

  void add_output_mapping(const std::string& op_id, size_t op_port, size_t pipeline_port) {
    auto it = operators_.find(op_id);
    if (it == operators_.end()) {
      throw std::runtime_error("Output operator not found: " + op_id);
    }
    if (pipeline_port >= num_output_ports()) {
      throw std::runtime_error("Invalid pipeline output port: " + std::to_string(pipeline_port));
    }
    RTBOT_LOG_DEBUG("Adding output mapping: ", op_id, ":", op_port, " -> ", pipeline_port);
    output_mappings_[op_id].emplace_back(op_port, pipeline_port);
  }

  void connect(const std::string& from_id, const std::string& to_id, size_t from_port = 0, size_t to_port = 0) {
    auto from_it = operators_.find(from_id);
    auto to_it = operators_.find(to_id);

    if (from_it == operators_.end() || to_it == operators_.end()) {
      throw std::runtime_error("Pipeline: invalid operator reference in connection from " + from_id + " to " + to_id);
    }

    RTBOT_LOG_DEBUG("Connecting operators: ", from_id, " -> ", to_id);
    from_it->second->connect(to_it->second, from_port, to_port);
    pipeline_connections_.push_back({from_id, to_id, from_port, to_port});
  }

  void reset() override {
    RTBOT_LOG_DEBUG("Resetting pipeline");
    for (auto& [_, op] : operators_) {
      op->reset();
    }
    has_key_ = false;
    current_key_ = 0.0;
    last_output_buffer_.clear();
  }

  void clear_all_output_ports() override {
    Operator::clear_all_output_ports();
    for (auto& [_, op] : operators_) {
      op->clear_all_output_ports();
    }
  }

  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();

    // Serialize segment state
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&has_key_),
                 reinterpret_cast<const uint8_t*>(&has_key_) + sizeof(has_key_));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&current_key_),
                 reinterpret_cast<const uint8_t*>(&current_key_) + sizeof(current_key_));

    // Serialize output buffer
    size_t buffer_size = last_output_buffer_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&buffer_size),
                 reinterpret_cast<const uint8_t*>(&buffer_size) + sizeof(buffer_size));

    for (const auto& [port, msg] : last_output_buffer_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port),
                   reinterpret_cast<const uint8_t*>(&port) + sizeof(port));
      Bytes msg_bytes = msg->serialize();
      size_t msg_size = msg_bytes.size();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&msg_size),
                   reinterpret_cast<const uint8_t*>(&msg_size) + sizeof(msg_size));
      bytes.insert(bytes.end(), msg_bytes.begin(), msg_bytes.end());
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    // Restore segment state
    std::memcpy(&has_key_, &(*it), sizeof(has_key_));
    it += sizeof(has_key_);
    std::memcpy(&current_key_, &(*it), sizeof(current_key_));
    it += sizeof(current_key_);

    // Restore output buffer
    size_t buffer_size;
    std::memcpy(&buffer_size, &(*it), sizeof(buffer_size));
    it += sizeof(buffer_size);

    last_output_buffer_.clear();
    for (size_t i = 0; i < buffer_size; ++i) {
      size_t port;
      std::memcpy(&port, &(*it), sizeof(port));
      it += sizeof(port);

      size_t msg_size;
      std::memcpy(&msg_size, &(*it), sizeof(msg_size));
      it += sizeof(msg_size);

      Bytes msg_bytes(it, it + msg_size);
      last_output_buffer_[port] = BaseMessage::deserialize(msg_bytes);
      it += msg_size;
    }
  }

  nlohmann::json collect() override {
    nlohmann::json result = {
      {"name", type_name()},
      {"bytes", bytes_to_base64(collect_bytes())}
    };

    nlohmann::json content;
    for (const auto& [op_id, op] : operators_) {
      content[op_id] = op->collect();
    }
    result["content"] = content;

    return result;
  }

  void restore_data_from_json(const nlohmann::json& j) override {
    Bytes bytes = base64_to_bytes(j.at("bytes").get<std::string>());
    auto it = bytes.cbegin();
    restore(it);

    const auto& content = j.at("content");
    for (auto& [op_id, op] : operators_) {
      op->restore_data_from_json(content.at(op_id));
    }
  }

  std::string type_name() const override { return "Pipeline"; }

  bool equals(const Pipeline& other) const {
    if (input_port_types_ != other.input_port_types_) return false;
    if (output_port_types_ != other.output_port_types_) return false;
    if (output_mappings_ != other.output_mappings_) return false;
    if ((bool)entry_operator_ != (bool)other.entry_operator_) return false;
    if (entry_operator_ && other.entry_operator_) {
      if (*entry_operator_ != *other.entry_operator_)
          return false;
    }
    if (operators_.size() != other.operators_.size()) return false;

    for (const auto& [key, op1] : operators_) {
        auto it = other.operators_.find(key);
        if (it == other.operators_.end()) return false;
        const auto& op2 = it->second;
        if (!op1 || !op2) return false;
        else if (*op1 != *op2) return false;
    }

    // Compare segment state
    if (has_key_ != other.has_key_) return false;
    if (has_key_ && current_key_ != other.current_key_) return false;
    if (last_output_buffer_.size() != other.last_output_buffer_.size()) return false;
    for (const auto& [port, msg] : last_output_buffer_) {
      auto oit = other.last_output_buffer_.find(port);
      if (oit == other.last_output_buffer_.end()) return false;
      if (msg->hash() != oit->second->hash()) return false;
      if (msg->time != oit->second->time) return false;
    }

    return Operator::equals(other);
  }

  bool operator==(const Pipeline& other) const {
    return equals(other);
  }

  bool operator!=(const Pipeline& other) const {
    return !(*this == other);
  }

 protected:
  void process_data(bool debug=false) override {
    if (!entry_operator_) {
      throw std::runtime_error("Pipeline entry point not configured");
    }

    auto& control_queue = get_control_queue(0);

    while (!control_queue.empty()) {
      // Check all data ports have at least one message
      bool all_data_ready = true;
      for (size_t i = 0; i < num_data_ports(); ++i) {
        if (get_data_queue(i).empty()) {
          all_data_ready = false;
          break;
        }
      }
      if (!all_data_ready) break;

      timestamp_t ctrl_time = control_queue.front()->time;

      // Find the minimum data timestamp across all data ports
      timestamp_t min_data_time = get_data_queue(0).front()->time;
      for (size_t i = 1; i < num_data_ports(); ++i) {
        timestamp_t t = get_data_queue(i).front()->time;
        if (t < min_data_time) min_data_time = t;
      }

      // Sync: discard older messages from whichever side is behind
      if (min_data_time < ctrl_time) {
        RTBOT_LOG_DEBUG("Pipeline: discarding data at t=", min_data_time, " (older than control t=", ctrl_time, ")");
        for (size_t i = 0; i < num_data_ports(); ++i) {
          if (!get_data_queue(i).empty() && get_data_queue(i).front()->time == min_data_time) {
            get_data_queue(i).pop_front();
          }
        }
        continue;
      }
      if (ctrl_time < min_data_time) {
        RTBOT_LOG_DEBUG("Pipeline: discarding control at t=", ctrl_time, " (older than data t=", min_data_time, ")");
        control_queue.pop_front();
        continue;
      }

      // Timestamps match: process this pair
      double new_key = dynamic_cast<const Message<NumberData>*>(control_queue.front().get())->data.value;
      timestamp_t boundary_time = ctrl_time;

      if (has_key_ && new_key != current_key_) {
        // Key changed: emit buffer with boundary timestamp, reset internals, start new segment
        emit_buffer(boundary_time);
        reset_internals();
      }
      if (!has_key_) has_key_ = true;
      current_key_ = new_key;
      forward_and_buffer(debug);

      // Consume the processed messages
      for (size_t i = 0; i < num_data_ports(); ++i) {
        get_data_queue(i).pop_front();
      }
      control_queue.pop_front();
    }
  }

 private:
  // Forward data from all input ports to the entry operator, execute the mesh,
  // and buffer any internal outputs (don't emit to pipeline output).
  void forward_and_buffer(bool debug) {
    for (size_t i = 0; i < num_data_ports(); ++i) {
      auto& msg = get_data_queue(i).front();
      entry_operator_->receive_data(msg->clone(), i);
    }
    entry_operator_->execute(debug);

    // Buffer output from mapped (terminal) operators
    for (const auto& [op_id, mappings] : output_mappings_) {
      auto it = operators_.find(op_id);
      if (it != operators_.end()) {
        auto& op = it->second;
        for (const auto& [operator_port, pipeline_port] : mappings) {
          if (operator_port < op->num_output_ports()) {
            const auto& source_queue = op->get_output_queue(operator_port);
            if (!source_queue.empty()) {
              last_output_buffer_[pipeline_port] = source_queue.back()->clone();
            }
          }
        }
      }
    }

    // Clear internal operator output queues (but NOT their state)
    clear_internal_output_ports();
  }

  // Emit buffered output to pipeline output ports, stamped with boundary timestamp
  void emit_buffer(timestamp_t boundary_time) {
    for (auto& [pipeline_port, msg] : last_output_buffer_) {
      if (pipeline_port < num_output_ports()) {
        auto output_msg = msg->clone();
        output_msg->time = boundary_time;
        RTBOT_LOG_DEBUG("Pipeline emitting buffer on port ", pipeline_port, " at time ", boundary_time);
        get_output_queue(pipeline_port).push_back(std::move(output_msg));
      }
    }
    last_output_buffer_.clear();
  }

  // Reset only internal operators (not Pipeline's own segment state)
  void reset_internals() {
    for (auto& [_, op] : operators_) {
      op->reset();
    }
  }

  // Clear only internal operators' output queues (not their internal state)
  void clear_internal_output_ports() {
    for (auto& [_, op] : operators_) {
      op->clear_all_output_ports();
    }
  }

  std::vector<std::string> input_port_types_;
  std::vector<std::string> output_port_types_;
  std::vector<PipelineConnection> pipeline_connections_;
  std::map<std::string, std::shared_ptr<Operator>> operators_;
  std::shared_ptr<Operator> entry_operator_;
  std::map<std::string, std::vector<std::pair<size_t, size_t>>> output_mappings_;

  // Segment state
  bool has_key_{false};
  double current_key_{0.0};
  std::map<size_t, std::unique_ptr<BaseMessage>> last_output_buffer_;
};

}  // namespace rtbot

#endif  // PIPELINE_H
