#ifndef PIPELINE_H
#define PIPELINE_H

#include <iostream>
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
      // Add data input port
      PortType::add_port(*this, type, true, false);
      input_port_types_.push_back(type);
    }

    // Configure output ports
    for (const auto& type : output_port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown output port type: " + type);
      }
      // Add output port
      PortType::add_port(*this, type, false, true);
      output_port_types_.push_back(type);
    }
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

  void set_entry(const std::string& op_id, size_t port = 0) {
    auto it = operators_.find(op_id);
    if (it == operators_.end()) {
      throw std::runtime_error("Entry operator not found: " + op_id);
    }
    entry_operator_ = it->second;
    entry_port_ = port;
    RTBOT_LOG_DEBUG("Setting entry operator: ", op_id, " -> ", port);
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
    // Add connection to pipeline
    pipeline_connections_.push_back({from_id, to_id, from_port, to_port});
  }

  void reset() override {
    RTBOT_LOG_DEBUG("Resetting pipeline");
    // Then reset all internal operators
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

  std::string type_name() const override { return "Pipeline"; }

 protected:
  void process_data() override {
    // Check if we have an entry point configured
    if (!entry_operator_) {
      throw std::runtime_error("Pipeline entry point not configured");
    }

    // Forward input messages to entry operator
    for (size_t i = 0; i < num_data_ports(); ++i) {
      auto& input_queue = get_data_queue(i);
      while (!input_queue.empty()) {
        auto& msg = input_queue.front();
        entry_operator_->receive_data(msg->clone(), i);
        entry_operator_->execute();
        input_queue.pop_front();
        // Process output mappings
        bool was_reset = false;
        for (const auto& [op_id, mappings] : output_mappings_) {
          auto it = operators_.find(op_id);
          if (it != operators_.end()) {
            auto& op = it->second;
            for (const auto& [operator_port, pipeline_port] : mappings) {
              if (operator_port < op->num_output_ports() && pipeline_port < num_output_ports()) {
                const auto& source_queue = op->get_output_queue(operator_port);
                // Only forward if source operator has produced output on the mapped port
                if (!source_queue.empty()) {
                  auto& target_queue = get_output_queue(pipeline_port);
                  for (const auto& msg : source_queue) {
                    RTBOT_LOG_DEBUG("Forwarding message ", msg->to_string(), " from ", op_id, " -> ", pipeline_port);
                    target_queue.push_back(msg->clone());
                    reset();
                    was_reset = true;
                    break;
                  }
                }
              }
              if (was_reset) {
                break;
              }
            }
          }
          if (was_reset) {
            break;
          }
        }
      }
    }
  }

 private:
  std::vector<std::string> input_port_types_;
  std::vector<std::string> output_port_types_;
  std::vector<PipelineConnection> pipeline_connections_;
  std::map<std::string, std::shared_ptr<Operator>> operators_;
  std::shared_ptr<Operator> entry_operator_;
  size_t entry_port_;
  std::map<std::string, std::vector<std::pair<size_t, size_t>>> output_mappings_;
};

}  // namespace rtbot

#endif  // PIPELINE_H