#ifndef PIPELINE_H
#define PIPELINE_H

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Pipeline : public Operator {
 public:
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

  // API for configuring the pipeline
  void register_operator(const std::string& id, std::shared_ptr<Operator> op) { operators_[id] = std::move(op); }

  void set_entry(const std::string& op_id, size_t port = 0) {
    auto it = operators_.find(op_id);
    if (it == operators_.end()) {
      throw std::runtime_error("Entry operator not found: " + op_id);
    }
    entry_operator_ = it->second;
    entry_port_ = port;
  }

  void add_output_mapping(const std::string& op_id, size_t op_port, size_t pipeline_port) {
    auto it = operators_.find(op_id);
    if (it == operators_.end()) {
      throw std::runtime_error("Output operator not found: " + op_id);
    }
    if (pipeline_port >= num_output_ports()) {
      throw std::runtime_error("Invalid pipeline output port: " + std::to_string(pipeline_port));
    }
    output_mappings_[op_id].emplace_back(op_port, pipeline_port);
  }

  void connect(const std::string& from_id, const std::string& to_id, size_t from_port = 0, size_t to_port = 0) {
    auto from_it = operators_.find(from_id);
    auto to_it = operators_.find(to_id);

    if (from_it == operators_.end() || to_it == operators_.end()) {
      throw std::runtime_error("Invalid operator reference in connection");
    }

    from_it->second->connect(to_it->second, from_port, to_port);
  }

  void reset() override {
    // First reset our own state
    Operator::reset();

    // Then reset all internal operators
    for (auto& [_, op] : operators_) {
      op->reset();
    }
  }

  void clear_all_output_ports() override {
    // Check if we produced any output
    bool has_output = false;
    for (size_t i = 0; i < num_output_ports(); ++i) {
      if (!get_output_queue(i).empty()) {
        has_output = true;
        break;
      }
    }

    // If we produced output, reset the pipeline for next iteration
    if (has_output) {
      reset();
    } else {
      // Just clear outputs if no processing happened
      Operator::clear_all_output_ports();
      for (auto& [_, op] : operators_) {
        op->clear_all_output_ports();
      }
    }
  }

  std::string type_name() const override { return "Pipeline"; }

 protected:
  void process_data() override {
    // Check if we have an entry point configured
    if (!entry_operator_) {
      throw std::runtime_error("Pipeline entry point not configured");
    }

    // Forward messages from each input port
    for (size_t i = 0; i < num_data_ports(); ++i) {
      auto& input_queue = get_data_queue(i);

      while (!input_queue.empty()) {
        const auto& msg = input_queue.front();

        // Forward message maintaining its type
        entry_operator_->receive_data(msg->clone(), entry_port_);
        entry_operator_->execute();
        input_queue.pop_front();
      }
    }

    // Collect results from output mappings
    for (const auto& [op_id, mappings] : output_mappings_) {
      auto& op = operators_[op_id];
      for (const auto& [op_port, pipeline_port] : mappings) {
        const auto& queue = op->get_output_queue(op_port);
        if (!queue.empty()) {
          for (const auto& msg : queue) {
            get_output_queue(pipeline_port).push_back(msg->clone());
          }
        }
      }
    }
  }

 private:
  std::vector<std::string> input_port_types_;
  std::vector<std::string> output_port_types_;
  std::map<std::string, std::shared_ptr<Operator>> operators_;
  std::shared_ptr<Operator> entry_operator_;
  size_t entry_port_;
  std::map<std::string, std::vector<std::pair<size_t, size_t>>> output_mappings_;
};

}  // namespace rtbot

#endif  // PIPELINE_H