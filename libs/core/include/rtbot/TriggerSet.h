#ifndef TRIGGER_SET_H
#define TRIGGER_SET_H

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Logger.h"
#include "rtbot/CompositeConnection.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class TriggerSet : public Operator {
 public:

  TriggerSet(std::string id, const std::string& input_port_type, const std::string& output_port_type)
      : Operator(std::move(id)),
        input_port_type_(input_port_type),
        output_port_type_(output_port_type) {
    // Configure the single input port
    if (!PortType::is_valid_port_type(input_port_type)) {
      throw std::runtime_error("Unknown input port type: " + input_port_type);
    }
    PortType::add_port(*this, input_port_type, true, false, false);

    // Configure the single output port
    if (!PortType::is_valid_port_type(output_port_type)) {
      throw std::runtime_error("Unknown output port type: " + output_port_type);
    }
    PortType::add_port(*this, output_port_type, false, false, true);
  }

  // Get configurations
  const std::string& get_input_port_type() const { return input_port_type_; }
  const std::string& get_output_port_type() const { return output_port_type_; }
  const std::map<std::string, std::shared_ptr<Operator>>& get_operators() const { return operators_; }
  const std::vector<CompositeConnection>& get_connections() const { return connections_; }
  const std::string& get_entry_operator_id() const { return entry_operator_->id(); }
  const std::string& get_output_operator_id() const { return output_operator_->id(); }
  size_t get_output_operator_port() const { return output_operator_port_; }

  // API for configuring the trigger set
  void register_operator(std::shared_ptr<Operator> op) { operators_[op->id()] = std::move(op); }

  void set_entry(const std::string& op_id) {
    auto it = operators_.find(op_id);
    if (it == operators_.end()) {
      throw std::runtime_error("Entry operator not found: " + op_id);
    }
    if (it->second->num_data_ports() < 1) {
      throw std::runtime_error("Entry operator must have at least one data port: " + op_id);
    }
    entry_operator_ = it->second;
    RTBOT_LOG_DEBUG("Setting entry operator: ", op_id);
  }

  void set_output(const std::string& op_id, size_t op_port = 0) {
    auto it = operators_.find(op_id);
    if (it == operators_.end()) {
      throw std::runtime_error("Output operator not found: " + op_id);
    }
    if (op_port >= it->second->num_output_ports()) {
      throw std::runtime_error("Invalid output operator port: " + std::to_string(op_port));
    }
    RTBOT_LOG_DEBUG("Setting output operator: ", op_id, ":", op_port);
    output_operator_ = it->second;
    output_operator_port_ = op_port;
  }

  void connect(const std::string& from_id, const std::string& to_id, size_t from_port = 0, size_t to_port = 0) {
    auto from_it = operators_.find(from_id);
    auto to_it = operators_.find(to_id);

    if (from_it == operators_.end() || to_it == operators_.end()) {
      throw std::runtime_error("TriggerSet: invalid operator reference in connection from " + from_id + " to " + to_id);
    }

    RTBOT_LOG_DEBUG("Connecting operators: ", from_id, " -> ", to_id);
    from_it->second->connect(to_it->second, from_port, to_port);
    connections_.push_back({from_id, to_id, from_port, to_port});
  }

  void reset() override {
    RTBOT_LOG_DEBUG("Resetting trigger set");
    for (auto& [_, op] : operators_) {
      op->reset();
    }
  }

  void clear_all_output_ports() override {
    Operator::clear_all_output_ports();
    for (auto& [_, op] : operators_) {
      op->clear_all_output_ports();
    }
  }

  nlohmann::json collect() override {
    nlohmann::json result = {
      {"name", type_name()},
      {"bytes", bytes_to_base64(Operator::collect_bytes())}
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
    Operator::restore(it);

    const auto& content = j.at("content");
    for (auto& [op_id, op] : operators_) {
      op->restore_data_from_json(content.at(op_id));
    }
  }

  std::string type_name() const override { return "TriggerSet"; }

  bool equals(const TriggerSet& other) const {
    if (input_port_type_ != other.input_port_type_) return false;
    if (output_port_type_ != other.output_port_type_) return false;
    if (output_operator_port_ != other.output_operator_port_) return false;
    if ((bool)entry_operator_ != (bool)other.entry_operator_) return false;
    if (entry_operator_ && other.entry_operator_) {
      if (*entry_operator_ != *other.entry_operator_)
          return false;
    }
    if ((bool)output_operator_ != (bool)other.output_operator_) return false;
    if (output_operator_ && other.output_operator_) {
      if (*output_operator_ != *other.output_operator_)
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

    return Operator::equals(other);
  }

  bool operator==(const TriggerSet& other) const {
    return equals(other);
  }

  bool operator!=(const TriggerSet& other) const {
    return !(*this == other);
  }

 protected:
  void process_data(bool debug = false) override {
    if (!entry_operator_) {
      throw std::runtime_error("TriggerSet entry point not configured");
    }
    if (!output_operator_) {
      throw std::runtime_error("TriggerSet output not configured");
    }

    // Forward input messages from the single input port to the entry operator's data port 0
    auto& input_queue = get_data_queue(0);
    while (!input_queue.empty()) {
      auto& msg = input_queue.front();
      entry_operator_->receive_data(msg->clone(), 0);
      entry_operator_->execute(debug);
      input_queue.pop_front();

      // Check the single output operator/port for a fired message
      const auto& source_queue = output_operator_->get_output_queue(output_operator_port_);
      if (!source_queue.empty()) {
        auto& target_queue = get_output_queue(0);
        const auto& fired = source_queue.front();
        RTBOT_LOG_DEBUG("Forwarding message ", fired->to_string(), " from ", output_operator_->id(),
                        ":", output_operator_port_, " -> 0");
        target_queue.push_back(fired->clone());
        // Reset the internal mesh: a single trigger consumes the cycle
        reset();
      }
    }
  }

 private:
  std::string input_port_type_;
  std::string output_port_type_;
  std::vector<CompositeConnection> connections_;
  std::map<std::string, std::shared_ptr<Operator>> operators_;
  std::shared_ptr<Operator> entry_operator_;
  std::shared_ptr<Operator> output_operator_;
  size_t output_operator_port_{0};
};

}  // namespace rtbot

#endif  // TRIGGER_SET_H
