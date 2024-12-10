#ifndef OPERATOR_H
#define OPERATOR_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/StateSerializer.h"
#include "rtbot/telemetry/OpenTelemetry.h"

namespace rtbot {

// Queue of messages for ports
using MessageQueue = std::deque<std::unique_ptr<BaseMessage>>;

// Port information
struct PortInfo {
  MessageQueue queue;
  std::type_index type;
};

// Base operator class
class Operator {
 public:
  Operator(std::string id) : id_(std::move(id)) {}
  virtual ~Operator() = default;

  virtual std::string type_name() const = 0;

  // Default implementations for core operator state
  virtual Bytes collect() {
    Bytes bytes;

    // Serialize port counts
    size_t data_ports_count = data_ports_.size();
    size_t control_ports_count = control_ports_.size();
    size_t output_ports_count = output_ports_.size();

    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&data_ports_count),
                 reinterpret_cast<const uint8_t*>(&data_ports_count) + sizeof(data_ports_count));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&control_ports_count),
                 reinterpret_cast<const uint8_t*>(&control_ports_count) + sizeof(control_ports_count));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&output_ports_count),
                 reinterpret_cast<const uint8_t*>(&output_ports_count) + sizeof(output_ports_count));

    // Serialize port types
    for (const auto& port : data_ports_) {
      StateSerializer::serialize_type_index(bytes, port.type);
    }
    for (const auto& port : control_ports_) {
      StateSerializer::serialize_type_index(bytes, port.type);
    }
    for (const auto& port : output_ports_) {
      StateSerializer::serialize_type_index(bytes, port.type);
    }

    // Serialize message queues
    for (const auto& port : data_ports_) {
      StateSerializer::serialize_message_queue(bytes, port.queue);
    }
    for (const auto& port : control_ports_) {
      StateSerializer::serialize_message_queue(bytes, port.queue);
    }
    for (const auto& port : output_ports_) {
      StateSerializer::serialize_message_queue(bytes, port.queue);
    }
    // Serialize connection stateful data
    for (const auto& conn : connections_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&conn.last_propagated_index),
                   reinterpret_cast<const uint8_t*>(&conn.last_propagated_index) + sizeof(conn.last_propagated_index));
    }

    // Serialize ports with new data sets
    StateSerializer::serialize_index_set(bytes, data_ports_with_new_data_);
    StateSerializer::serialize_index_set(bytes, control_ports_with_new_data_);

    return bytes;
  }

  virtual void restore(Bytes::const_iterator& it) {
    // Read port counts
    size_t data_ports_count = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);
    size_t control_ports_count = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);
    size_t output_ports_count = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    // Validate port counts match current configuration
    StateSerializer::validate_port_count(data_ports_count, data_ports_.size(), "Data");
    StateSerializer::validate_port_count(control_ports_count, control_ports_.size(), "Control");
    StateSerializer::validate_port_count(output_ports_count, output_ports_.size(), "Output");

    // Validate and restore port types
    for (auto& port : data_ports_) {
      StateSerializer::validate_and_restore_type(it, port.type);
    }
    for (auto& port : control_ports_) {
      StateSerializer::validate_and_restore_type(it, port.type);
    }
    for (auto& port : output_ports_) {
      StateSerializer::validate_and_restore_type(it, port.type);
    }

    // Restore message queues
    for (auto& port : data_ports_) {
      StateSerializer::deserialize_message_queue(it, port.queue);
    }
    for (auto& port : control_ports_) {
      StateSerializer::deserialize_message_queue(it, port.queue);
    }
    for (auto& port : output_ports_) {
      StateSerializer::deserialize_message_queue(it, port.queue);
    }

    // Restore connections (excluding child pointers)
    for (size_t i = 0; i < connections_.size(); ++i) {
      connections_[i].last_propagated_index = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);
    }

    // Restore ports with new data sets
    StateSerializer::deserialize_index_set(it, data_ports_with_new_data_);
    StateSerializer::deserialize_index_set(it, control_ports_with_new_data_);
  }

  // Dynamic port management with type information
  template <typename T>
  void add_data_port() {
    data_ports_.push_back({MessageQueue{}, std::type_index(typeid(T))});
  }

  template <typename T>
  void add_control_port() {
    control_ports_.push_back({MessageQueue{}, std::type_index(typeid(T))});
  }

  template <typename T>
  void add_output_port() {
    output_ports_.push_back({MessageQueue{}, std::type_index(typeid(T))});
  }

  size_t num_data_ports() const { return data_ports_.size(); }
  size_t num_control_ports() const { return control_ports_.size(); }
  size_t num_output_ports() const { return output_ports_.size(); }

  // Runtime port access for data with type checking
  virtual void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index) {
    if (port_index >= data_ports_.size()) {
      throw std::runtime_error("Invalid data port index");
    }

    if (msg->type() != data_ports_[port_index].type) {
      throw std::runtime_error("Type mismatch on data port");
    }

#ifdef RTBOT_INSTRUMENTATION
    RTBOT_RECORD_MESSAGE(id_, type_name(), std::move(msg->clone()));
#endif

    data_ports_[port_index].queue.push_back(std::move(msg));
    data_ports_with_new_data_.insert(port_index);
  }

  virtual void reset() {
    // Base reset implementation clears all queues
    for (auto& port : data_ports_) {
      port.queue.clear();
    }
    for (auto& port : control_ports_) {
      port.queue.clear();
    }
    for (auto& port : output_ports_) {
      port.queue.clear();
    }
    data_ports_with_new_data_.clear();
    control_ports_with_new_data_.clear();
  }

  // This should be called by the runtime to clear all output ports before executing
  // the operator again
  virtual void clear_all_output_ports() {
    for (auto& port : output_ports_) {
      port.queue.clear();
    }
    // also clear all connections
    for (auto& conn : connections_) {
      conn.last_propagated_index = 0;
    }
  }

  void execute() {
    SpanScope span_scope{"operator_execute"};
    RTBOT_ADD_ATTRIBUTE("operator.id", id_);

    if (data_ports_with_new_data_.empty() && control_ports_with_new_data_.empty()) {
      return;
    }

    // Process control messages first
    if (!control_ports_with_new_data_.empty()) {
      SpanScope control_scope{"process_control"};
      process_control();
      control_ports_with_new_data_.clear();
    }

    // Then process data
    process_data();
    data_ports_with_new_data_.clear();

#ifdef RTBOT_INSTRUMENTATION
    for (size_t i = 0; i < output_ports_.size(); i++) {
      for (const auto& msg : output_ports_[i].queue) {
        RTBOT_RECORD_OPERATOR_OUTPUT(id_, type_name(), i, std::move(msg->clone()));
      }
    }
#endif

    propagate_outputs();
  }

  // Runtime port access for control messages with type checking
  virtual void receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index) {
    if (port_index >= control_ports_.size()) {
      throw std::runtime_error("Invalid control port index");
    }

    if (msg->type() != control_ports_[port_index].type) {
      throw std::runtime_error("Type mismatch on control port");
    }

    control_ports_[port_index].queue.push_back(std::move(msg));
    control_ports_with_new_data_.insert(port_index);
  }

  std::shared_ptr<Operator> connect(std::shared_ptr<Operator> child, size_t output_port = 0,
                                    size_t child_input_port = 0) {
    if (output_port >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index");
    }

    // Type check the connection
    if (output_ports_[output_port].type != child->data_ports_[child_input_port].type) {
      throw std::runtime_error("Type mismatch in operator connection " + id_ + " -> " + child->id_);
    }

    connections_.push_back({child, output_port, child_input_port, 0});
    return child;
  }

  // Get port type
  std::type_index get_data_port_type(size_t port_index) const {
    if (port_index >= data_ports_.size()) {
      throw std::runtime_error("Invalid data port index");
    }
    return data_ports_[port_index].type;
  }

  std::type_index get_control_port_type(size_t port_index) const {
    if (port_index >= control_ports_.size()) {
      throw std::runtime_error("Invalid control port index");
    }
    return control_ports_[port_index].type;
  }

  std::type_index get_output_port_type(size_t port_index) const {
    if (port_index >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index");
    }
    return output_ports_[port_index].type;
  }

  const std::string& id() const { return id_; }

  // Access to port queues for derived classes
  MessageQueue& get_data_queue(size_t port_index) {
    if (port_index >= data_ports_.size()) {
      throw std::runtime_error("Invalid data port index");
    }
    return data_ports_[port_index].queue;
  }

  MessageQueue& get_control_queue(size_t port_index) {
    if (port_index >= control_ports_.size()) {
      throw std::runtime_error("Invalid control port index");
    }
    return control_ports_[port_index].queue;
  }

  MessageQueue& get_output_queue(size_t port_index) {
    if (port_index >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index");
    }
    return output_ports_[port_index].queue;
  }

  const MessageQueue& get_output_queue(size_t port_index) const {
    if (port_index >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index");
    }
    return output_ports_[port_index].queue;
  }

 protected:
  virtual void process_data() = 0;
  virtual void process_control() {}

  void propagate_outputs() {
    for (auto& conn : connections_) {
      auto& output_queue = output_ports_[conn.output_port].queue;
      size_t last_propagated_index = conn.last_propagated_index;

      if (output_queue.empty()) {
        continue;
      }

      for (size_t i = last_propagated_index; i < output_queue.size(); i++) {
        auto msg_copy = output_queue[i]->clone();
#ifdef RTBOT_INSTRUMENTATION
        RTBOT_RECORD_MESSAGE_SENT(id_, type_name(), conn.child->id(), conn.child->type_name(),
                                  output_queue[i]->clone());
#endif
        conn.child->receive_data(std::move(msg_copy), conn.child_input_port);
      }

      conn.last_propagated_index = output_queue.size();
      conn.child->execute();
    }
  }

  struct Connection {
    std::shared_ptr<Operator> child;
    size_t output_port;
    size_t child_input_port;
    size_t last_propagated_index{0};  // Track last propagated message per connection
  };

  std::string id_;
  std::vector<PortInfo> data_ports_;
  std::vector<PortInfo> control_ports_;
  std::vector<PortInfo> output_ports_;
  std::vector<Connection> connections_;
  std::set<size_t> data_ports_with_new_data_;
  std::set<size_t> control_ports_with_new_data_;
};

}  // namespace rtbot

#endif  // OPERATOR_H
