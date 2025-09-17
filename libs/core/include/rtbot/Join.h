#ifndef JOIN_H
#define JOIN_H

#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"
#include "rtbot/TimestampTracker.h"

namespace rtbot {

class Join : public Operator {
 public:
  // Base constructor with separate input and output port specifications
  Join(std::string id, const std::vector<std::string>& input_port_types,
       const std::vector<std::string>& output_port_types)
      : Operator(std::move(id)) {
    if (input_port_types.size() < 2) {
      throw std::runtime_error("Join requires at least 2 input ports");
    }

    // Add input ports and initialize tracking
    for (const auto& type : input_port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown port type: " + type);
      }

      PortType::add_port(*this, type, true, false, false);  // input only
      data_time_tracker_[num_data_ports() - 1] = std::set<timestamp_t>();
      port_type_names_.push_back(type);
    }

    // Add output ports
    for (const auto& type : output_port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown port type: " + type);
      }
      PortType::add_port(*this, type, false, false, true);  // output only
    }
  }

  // Constructor with port types only
  Join(std::string id, const std::vector<std::string>& port_types) : Operator(std::move(id)) {
    if (port_types.size() < 2) {
      throw std::runtime_error("Join requires at least 2 input ports");
    }

    for (const auto& type : port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown port type: " + type);
      }

      PortType::add_port(*this, type, true, false, true);
      data_time_tracker_[num_data_ports() - 1] = std::set<timestamp_t>();
      port_type_names_.push_back(type);
    }
  }

  // Constructor with number of ports of the same type
  template <typename T>
  Join(std::string id, size_t num_ports) : Operator(std::move(id)) {
    if (num_ports < 2) {
      throw std::runtime_error("Join requires at least 2 input ports");
    }

    std::string port_type = PortType::get_port_type<T>();
    for (size_t i = 0; i < num_ports; ++i) {
      PortType::add_port(*this, port_type, true, false, true);
      data_time_tracker_[i] = std::set<timestamp_t>();
      port_type_names_.push_back(port_type);
    }
  }

  std::string type_name() const override { return "Join"; }

  // Get port configuration
  const std::vector<std::string>& get_port_types() const { return port_type_names_; }

  Bytes collect() override {
    // First collect base state
    Bytes bytes = Operator::collect();

    // Serialize data time tracker
    StateSerializer::serialize_port_timestamp_set_map(bytes, data_time_tracker_);

    // Serialize port type names
    StateSerializer::serialize_string_vector(bytes, port_type_names_);

    // Serialize synchronized_data
    size_t sync_size = synchronized_data.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sync_size),
                 reinterpret_cast<const uint8_t*>(&sync_size) + sizeof(sync_size));

    for (const auto& [time, messages] : synchronized_data) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                   reinterpret_cast<const uint8_t*>(&time) + sizeof(time));

      size_t msg_count = messages.size();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&msg_count),
                   reinterpret_cast<const uint8_t*>(&msg_count) + sizeof(msg_count));

      for (const auto& msg : messages) {
        Bytes msg_bytes = msg->serialize();
        size_t msg_size = msg_bytes.size();
        bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&msg_size),
                     reinterpret_cast<const uint8_t*>(&msg_size) + sizeof(msg_size));
        bytes.insert(bytes.end(), msg_bytes.begin(), msg_bytes.end());
      }
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    // First restore base state
    Operator::restore(it);

    // Clear current state
    data_time_tracker_.clear();

    // Restore data time tracker
    StateSerializer::deserialize_port_timestamp_set_map(it, data_time_tracker_);

    // Validate port count
    StateSerializer::validate_port_count(data_time_tracker_.size(), num_data_ports(), "Data");

    // Restore port type names
    StateSerializer::deserialize_string_vector(it, port_type_names_);

    // Restore synchronized_data
    synchronized_data.clear();
    size_t sync_size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    for (size_t i = 0; i < sync_size; ++i) {
      timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
      it += sizeof(timestamp_t);

      size_t msg_count = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);

      std::vector<std::unique_ptr<BaseMessage>> messages;
      for (size_t j = 0; j < msg_count; ++j) {
        size_t msg_size = *reinterpret_cast<const size_t*>(&(*it));
        it += sizeof(size_t);

        Bytes msg_bytes(it, it + msg_size);
        messages.push_back(BaseMessage::deserialize(msg_bytes));
        it += msg_size;
      }
      synchronized_data[time] = std::move(messages);
    }

    // Validate port types match
    if (port_type_names_.size() != num_data_ports()) {
      throw std::runtime_error("Port type count mismatch during restore");
    }
  }

  void reset() override {
    Operator::reset();
    for (auto& [_, tracker] : data_time_tracker_) {
      tracker.clear();
    }
    synchronized_data.clear();
  }

  void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    auto time = msg->time;
    Operator::receive_data(std::move(msg), port_index);

    // Track timestamp
    data_time_tracker_[port_index].insert(time);
  }

 protected:
  // Performs synchronization of input messages
  void sync() {
    while (true) {
      // Find oldest common timestamp across all ports
      auto common_time = TimestampTracker::find_oldest_common_time(data_time_tracker_);
      if (!common_time) {
        break;
      }

      // Process messages with common timestamp
      std::vector<std::unique_ptr<BaseMessage>> synced_messages;

      for (size_t port = 0; port < num_data_ports(); ++port) {
        auto& queue = get_data_queue(port);
        bool message_found = false;

        // Find message with matching timestamp
        while (!queue.empty()) {
          const auto& front_msg = queue.front();
          if (front_msg->time < *common_time) {
            data_time_tracker_[port].erase(front_msg->time);
            queue.pop_front();
          } else if (front_msg->time == *common_time) {
            synced_messages.push_back(front_msg->clone());
            data_time_tracker_[port].erase(front_msg->time);
            queue.pop_front();
            message_found = true;
            break;
          } else {
            break;
          }
        }

        if (!message_found) {
          throw std::runtime_error("Implementation error in Join: TimestampTracker found common timestamp " +
                                   std::to_string(*common_time) + " but message not found in queue for port " +
                                   std::to_string(port));
        }
      }

      // Store synchronized messages
      synchronized_data[*common_time] = std::move(synced_messages);
    }
  }

  void process_data() override {
    // First perform synchronization
    sync();

    // Then process synchronized data
    for (const auto& [time, messages] : synchronized_data) {
      for (size_t i = 0; i < messages.size(); ++i) {
        get_output_queue(i).push_back(messages[i]->clone());
      }
    }

    // Clear synchronized data after processing
    synchronized_data.clear();
  }

  void process_control() override {}  // Join has no control ports

  // Access to synchronized data for derived classes
  const std::map<timestamp_t, std::vector<std::unique_ptr<BaseMessage>>>& get_synchronized_data() const {
    return synchronized_data;
  }

  void clear_synchronized_data() { synchronized_data.clear(); }

 private:
  std::vector<std::string> port_type_names_;
  std::map<size_t, std::set<timestamp_t>> data_time_tracker_;
  std::map<timestamp_t, std::vector<std::unique_ptr<BaseMessage>>> synchronized_data;
};

// Factory functions remain unchanged
inline std::shared_ptr<Join> make_join(std::string id, const std::vector<std::string>& port_types) {
  return std::make_shared<Join>(std::move(id), port_types);
}

template <typename T>
inline std::shared_ptr<Join> make_binary_join(std::string id) {
  return std::make_shared<Join>(std::move(id), 2);
}

inline std::shared_ptr<Join> make_number_join(std::string id, size_t num_ports) {
  return std::make_shared<Join>(std::move(id), std::vector<std::string>(num_ports, PortType::NUMBER));
}

inline std::shared_ptr<Join> make_boolean_join(std::string id, size_t num_ports) {
  return std::make_shared<Join>(std::move(id), std::vector<std::string>(num_ports, PortType::BOOLEAN));
}

}  // namespace rtbot

#endif  // JOIN_H