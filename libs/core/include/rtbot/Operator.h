#ifndef OPERATOR_H
#define OPERATOR_H

#define MAX_SIZE_PER_PORT 17280

#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_set>
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
  timestamp_t last_timestamp{std::numeric_limits<timestamp_t>::min()};
};

enum class PortKind { DATA, CONTROL };

// Base operator class
class Operator {
 public:
  Operator(std::string id) : id_(std::move(id)), max_size_per_port_(MAX_SIZE_PER_PORT) {}
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
    // Serialize message queues
    for (const auto& port : data_ports_) {
      StateSerializer::serialize_message_queue(bytes, port.queue);
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port.last_timestamp),
                   reinterpret_cast<const uint8_t*>(&port.last_timestamp) + sizeof(port.last_timestamp));
    }
    for (const auto& port : control_ports_) {
      StateSerializer::serialize_message_queue(bytes, port.queue);
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port.last_timestamp),
                   reinterpret_cast<const uint8_t*>(&port.last_timestamp) + sizeof(port.last_timestamp));
    }
    for (const auto& port : output_ports_) {
      StateSerializer::serialize_message_queue(bytes, port.queue);
    }    

    return bytes;
  }

  virtual void restore(Bytes::const_iterator& it) {
    // ---- Read port counts safely ----
    size_t data_ports_count;
    std::memcpy(&data_ports_count, &(*it), sizeof(data_ports_count));
    it += sizeof(size_t);

    size_t control_ports_count;
    std::memcpy(&control_ports_count, &(*it), sizeof(control_ports_count));
    it += sizeof(size_t);

    size_t output_ports_count;
    std::memcpy(&output_ports_count, &(*it), sizeof(output_ports_count));
    it += sizeof(size_t);

    // ---- Validate counts ----
    StateSerializer::validate_port_count(data_ports_count, data_ports_.size(), "Data");
    StateSerializer::validate_port_count(control_ports_count, control_ports_.size(), "Control");
    StateSerializer::validate_port_count(output_ports_count, output_ports_.size(), "Output");

    // ---- Restore message queues ----
    for (auto& port : data_ports_) {
        StateSerializer::deserialize_message_queue(it, port.queue);
        std::memcpy(&port.last_timestamp, &(*it), sizeof(port.last_timestamp));
        it += sizeof(timestamp_t);
    }
    for (auto& port : control_ports_) {
        StateSerializer::deserialize_message_queue(it, port.queue);
        std::memcpy(&port.last_timestamp, &(*it), sizeof(port.last_timestamp));
        it += sizeof(timestamp_t);
    }
    for (auto& port : output_ports_) {
        StateSerializer::deserialize_message_queue(it, port.queue);
    }

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
  size_t max_size_per_port() const { return max_size_per_port_; }

  // Runtime port access for data with type checking
  virtual void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index) {
    if (port_index >= data_ports_.size()) {
      throw std::runtime_error("Invalid data port index at " + type_name() + "(" + id_ + ")" + ":" +
                               std::to_string(port_index));
    }

    if (msg->type() != data_ports_[port_index].type) {
      throw std::runtime_error("Type mismatch on data port at " + type_name() + "(" + id_ + ")" + ":" +
                               std::to_string(port_index));
    }

    // Check timestamp ordering
    if (msg->time <= data_ports_[port_index].last_timestamp) {
      throw std::runtime_error("Out of order timestamp received at " + type_name() + "(" + id_ + ")" + " port " +
                               std::to_string(port_index) + ". Current timestamp: " + std::to_string(msg->time) +
                               ", Last timestamp: " + std::to_string(data_ports_[port_index].last_timestamp));
    }

    // Update last timestamp
    data_ports_[port_index].last_timestamp = msg->time;

#ifdef RTBOT_INSTRUMENTATION
    RTBOT_RECORD_MESSAGE(id_, type_name(), std::move(msg->clone()));
#endif
    

    if (data_ports_[port_index].queue.size() == max_size_per_port_) {      
      data_ports_[port_index].queue.pop_front();
    }

    data_ports_[port_index].queue.push_back(std::move(msg));
  }

  virtual void reset() {
    // Base reset implementation clears all queues
    for (auto& port : data_ports_) {
      port.last_timestamp = std::numeric_limits<timestamp_t>::min();
      port.queue.clear();
    }
    for (auto& port : control_ports_) {
      port.last_timestamp = std::numeric_limits<timestamp_t>::min();
      port.queue.clear();
    }
    for (auto& port : output_ports_) {
      port.queue.clear();
    }
    for (auto& queue : debug_output_queues_) {
      queue.clear();
    }
  }

  // This should be called by the runtime to clear all output ports before executing
  // the operator again
  virtual void clear_all_output_ports() {
    for (auto& port : output_ports_) {
      port.queue.clear();
    }
    for (auto& queue : debug_output_queues_) {
      queue.clear();
    }
  }

  void execute(bool debug=false) {
    SpanScope span_scope{"operator_execute"};
    RTBOT_ADD_ATTRIBUTE("operator.id", id_);    

    // Process control messages first
    if (num_control_ports() > 0) {
      SpanScope control_scope{"process_control"};
      process_control(debug);      
    }

    // Then process data
    if (num_data_ports() > 0) {
      SpanScope data_scope{"process_data"};
      process_data(debug);      
    }

#ifdef RTBOT_INSTRUMENTATION
    for (size_t i = 0; i < output_ports_.size(); i++) {
      for (const auto& msg : output_ports_[i].queue) {
        RTBOT_RECORD_OPERATOR_OUTPUT(id_, type_name(), i, std::move(msg->clone()));
      }
    }
#endif

    propagate_outputs(debug);
  }

  // Runtime port access for control messages with type checking
  virtual void receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index) {
    if (port_index >= control_ports_.size()) {
      throw std::runtime_error("Invalid control port index at " + type_name() + "(" + id_ + ")" + ":" +
                               std::to_string(port_index));
    }

    if (msg->type() != control_ports_[port_index].type) {
      throw std::runtime_error("Type mismatch on control port at " + type_name() + "(" + id_ + ")" + ":" +
                               std::to_string(port_index));
    }

    // Check timestamp ordering
    if (msg->time <= control_ports_[port_index].last_timestamp) {
      throw std::runtime_error("Out of order timestamp received at " + type_name() + "(" + id_ + ")" +
                               " control port " + std::to_string(port_index) +
                               ". Current timestamp: " + std::to_string(msg->time) +
                               ", Last timestamp: " + std::to_string(control_ports_[port_index].last_timestamp));
    }

    // Update last timestamp
    control_ports_[port_index].last_timestamp = msg->time;

    
    if (control_ports_[port_index].queue.size() == max_size_per_port_) {      
      control_ports_[port_index].queue.pop_front();
    }

    control_ports_[port_index].queue.push_back(std::move(msg));

  }

  std::shared_ptr<Operator> connect(std::shared_ptr<Operator> child, size_t output_port = 0,
                                    size_t child_port_index = 0, PortKind child_port_kind = PortKind::DATA) {
    if (output_port >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index");
    }

    // Type check based on port kind
    if (child_port_kind == PortKind::DATA) {
      if (child_port_index >= child->data_ports_.size()) {
        throw std::runtime_error("Invalid child data port index " + std::to_string(child_port_index) +
                                 ", available data ports: " + std::to_string(child->data_ports_.size()) +
                                 ", found while connecting " + id_ + ":" + std::to_string(output_port) + " -> " +
                                 child->id_ + ":" + std::to_string(child_port_index));
      }
      if (output_ports_[output_port].type != child->data_ports_[child_port_index].type) {
        throw std::runtime_error(
            "Input port type mismatch in operator connection " + id_ + ":" + std::to_string(output_port) + " -> " +
            child->id_ + ":" + std::to_string(child_port_index) + " expected " +
            child->data_ports_[child_port_index].type.name() + " but got " + output_ports_[output_port].type.name());
      }
    } else {
      if (child_port_index >= child->control_ports_.size()) {
        throw std::runtime_error("Invalid child control port index " + std::to_string(child_port_index) +
                                 ", available control ports: " + std::to_string(child->control_ports_.size()) +
                                 ", found while connecting " + id_ + ":" + std::to_string(output_port) + " -> " +
                                 child->id_ + ":" + std::to_string(child_port_index));
      }
      if (output_ports_[output_port].type != child->control_ports_[child_port_index].type) {
        throw std::runtime_error(
            "Control port type mismatch in operator connection " + id_ + ":" + std::to_string(output_port) + " -> " +
            child->id_ + ":" + std::to_string(child_port_index) + " expected " +
            child->control_ports_[child_port_index].type.name() + " but got " + output_ports_[output_port].type.name());
      }
    }

    connections_.push_back({child, output_port, child_port_index, child_port_kind});
    return child;
  }

  // Get port type
  std::type_index get_data_port_type(size_t port_index) const {
    if (port_index >= data_ports_.size()) {
      throw std::runtime_error("Invalid data port index for data queue");
    }
    return data_ports_[port_index].type;
  }

  std::type_index get_control_port_type(size_t port_index) const {
    if (port_index >= control_ports_.size()) {
      throw std::runtime_error("Invalid control port index for control queue");
    }
    return control_ports_[port_index].type;
  }

  std::type_index get_output_port_type(size_t port_index) const {
    if (port_index >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index for output queue");
    }
    return output_ports_[port_index].type;
  }

  const std::string& id() const { return id_; }

  bool equals(const Operator& other) const {
      // Compare IDs
      if (id_ != other.id_) return false;
      if (type_name() != other.type_name()) return false;

      // Compare number of ports
      if (data_ports_.size() != other.data_ports_.size()) return false;
      if (control_ports_.size() != other.control_ports_.size()) return false;
      if (output_ports_.size() != other.output_ports_.size()) return false;

      // Compare data port types and last timestamps
      for (size_t i = 0; i < data_ports_.size(); ++i) {
        if (data_ports_[i].type != other.data_ports_[i].type) return false;
        if (data_ports_[i].last_timestamp != other.data_ports_[i].last_timestamp) return false;

        // Optional: compare queues
        if (data_ports_[i].queue.size() != other.data_ports_[i].queue.size()) return false;
        for (size_t j = 0; j < data_ports_[i].queue.size(); ++j) {
            if (data_ports_[i].queue[j]->hash() != other.data_ports_[i].queue[j]->hash()) return false;
            if (data_ports_[i].queue[j]->time != other.data_ports_[i].queue[j]->time) return false;
        }
      }

      // Compare control port types and last timestamps
      for (size_t i = 0; i < control_ports_.size(); ++i) {
        if (control_ports_[i].type != other.control_ports_[i].type) return false;
        if (control_ports_[i].last_timestamp != other.control_ports_[i].last_timestamp) return false;

        if (control_ports_[i].queue.size() != other.control_ports_[i].queue.size()) return false;
        for (size_t j = 0; j < control_ports_[i].queue.size(); ++j) {
            if (control_ports_[i].queue[j]->hash() != other.control_ports_[i].queue[j]->hash()) return false;
            if (control_ports_[i].queue[j]->time != other.control_ports_[i].queue[j]->time) return false;
        }
      }

      // Compare output ports queues (types can be ignored if always match)
      for (size_t i = 0; i < output_ports_.size(); ++i) {
        if (output_ports_[i].queue.size() != other.output_ports_[i].queue.size()) return false;
        for (size_t j = 0; j < output_ports_[i].queue.size(); ++j) {
            if (output_ports_[i].queue[j]->hash() != other.output_ports_[i].queue[j]->hash()) return false;
            if (output_ports_[i].queue[j]->time != other.output_ports_[i].queue[j]->time) return false;
        }
      }

      return true;
  }
  
  bool operator==(const Operator& other) const {
    return equals(other);
  }

  bool operator!=(const Operator& other) const {
    return !(*this == other);
  }

  // Access to port queues for derived classes
  MessageQueue& get_data_queue(size_t port_index) {
    if (port_index >= data_ports_.size()) {
      throw std::runtime_error("Invalid data port index for data queue");
    }
    return data_ports_[port_index].queue;
  }

  MessageQueue& get_control_queue(size_t port_index) {
    if (port_index >= control_ports_.size()) {
      throw std::runtime_error("Invalid control port index for control queue");
    }
    return control_ports_[port_index].queue;
  }

  MessageQueue& get_output_queue(size_t port_index) {
    if (port_index >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index for output queue");
    }
    return output_ports_[port_index].queue;
  }

  MessageQueue& get_debug_output_queue(size_t port_index) {
    static MessageQueue empty;
    if (port_index >= debug_output_queues_.size()) {
      return empty;
    }
    return debug_output_queues_[port_index];
  }

  const MessageQueue& get_output_queue(size_t port_index) const {
    if (port_index >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index for output queue");
    }
    return output_ports_[port_index].queue;
  }

 protected:
  virtual void process_data(bool debug) = 0;
  virtual void process_control(bool debug=false) {};

  bool sync_data_inputs() {

    if (data_ports_.empty()) return false;

    while (true) {
      // If any queue is empty, sync not possible
      for (auto& port : data_ports_) {
        if (port.queue.empty())
          return false;
      }

      // Find min and max front timestamps
      timestamp_t min_time = data_ports_.front().queue.front()->time;
      timestamp_t max_time = min_time;

      for (auto& port : data_ports_) {
        timestamp_t t = port.queue.front()->time;
        if (t < min_time) min_time = t;
        if (t > max_time) max_time = t;
      }

      // All equal → synchronized
      if (min_time == max_time)
        return true;

      // Pop all queues that have the oldest front timestamp
      for (auto& port : data_ports_) {
        if (!port.queue.empty() && port.queue.front()->time == min_time)
          port.queue.pop_front();
      }

      // If any queue now empty → cannot sync
      for (auto& port : data_ports_) {
        if (port.queue.empty())
          return false;
      }
    }
    return false;  
  }

  bool sync_control_inputs() {

    if (control_ports_.empty()) return false;

    while (true) {
      // If any queue is empty, sync not possible
      for (auto& port : control_ports_) {
        if (port.queue.empty())
          return false;
      }

      // Find min and max front timestamps
      timestamp_t min_time = control_ports_.front().queue.front()->time;
      timestamp_t max_time = min_time;

      for (auto& port : control_ports_) {
        timestamp_t t = port.queue.front()->time;
        if (t < min_time) min_time = t;
        if (t > max_time) max_time = t;
      }

      // All equal → synchronized
      if (min_time == max_time)
        return true;

      // Pop all queues that have the oldest front timestamp
      for (auto& port : control_ports_) {
        if (!port.queue.empty() && port.queue.front()->time == min_time)
          port.queue.pop_front();
      }

      // If any queue now empty → cannot sync
      for (auto& port : control_ports_) {
        if (port.queue.empty())
          return false;
      }
    }
    return false;  
  }

  void propagate_outputs(bool debug=false) {
    
    std::unordered_set<size_t> propagated_outputs;
    if (debug) {
      
      if (debug_output_queues_.size() != num_output_ports()) {
        debug_output_queues_.clear();
        for (int i = 0; i < num_output_ports(); i++) {
          debug_output_queues_.push_back(MessageQueue());
        }
      }
      
    } else if (!debug && debug_output_queues_.size() > 0) {
      debug_output_queues_.clear();
    }

    // Send the messages to the connected operators
    for (auto& conn : connections_) {      
      auto& output_queue = output_ports_[conn.output_port].queue;
      if (output_queue.empty()) {
        continue;
      }

      for (size_t i = 0; i < output_queue.size(); i++) {
        auto msg_copy = output_queue[i]->clone();
#ifdef RTBOT_INSTRUMENTATION
        RTBOT_RECORD_MESSAGE_SENT(id_, type_name(), std::to_string(i), conn.child->id(), conn.child->type_name(),
                                  std::to_string(conn.child_input_port),
                                  conn.child_port_kind == PortKind::DATA ? "" : "[c]", output_queue[i]->clone());
#endif
        // Route message based on connection port kind
        if (conn.child_port_kind == PortKind::DATA) {
          conn.child->receive_data(std::move(msg_copy), conn.child_input_port);
        } else {
          conn.child->receive_control(std::move(msg_copy), conn.child_input_port);
        }
         propagated_outputs.insert(conn.output_port);
      }
    }
    
    if (debug) {
      for (size_t i = 0; i < num_output_ports(); i++) {
        auto& queue = output_ports_[i].queue;
        for (size_t j = 0; j < queue.size(); j++) {
          auto msg_copy = queue[j]->clone();
          debug_output_queues_[i].push_back(std::move(msg_copy));
        }
      }
    }

    for (const size_t& value : propagated_outputs) {
      get_output_queue(value).clear();
    }

    

    // Then execute connected operators
    for (auto& conn : connections_) {      
      if (conn.child != nullptr && propagated_outputs.find(conn.output_port) != propagated_outputs.end())
      conn.child->execute(debug);
    }

  }
  struct Connection {
    std::shared_ptr<Operator> child;
    size_t output_port;
    size_t child_input_port;    
    PortKind child_port_kind{PortKind::DATA};
  };

  std::string id_;
  std::vector<PortInfo> data_ports_;
  std::vector<PortInfo> control_ports_;
  std::vector<PortInfo> output_ports_;
  std::vector<MessageQueue> debug_output_queues_;
  std::vector<Connection> connections_;  
  std::size_t max_size_per_port_;
};

}  // namespace rtbot

#endif  // OPERATOR_H
