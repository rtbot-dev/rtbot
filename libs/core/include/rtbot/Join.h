#ifndef JOIN_H
#define JOIN_H

#include <map>
#include <string>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"
#include "rtbot/TimestampTracker.h"

namespace rtbot {

class Join : public Operator {
 public:
  // Constructor with port types
  Join(std::string id, const std::vector<std::string>& port_types) : Operator(std::move(id)) {
    if (port_types.size() < 2) {
      throw std::runtime_error("Join requires at least 2 input ports");
    }

    for (const auto& type : port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown port type: " + type);
      }

      PortType::add_port(*this, type, true, true);
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
      PortType::add_port(*this, port_type, true, true);
      data_time_tracker_[i] = std::set<timestamp_t>();
      port_type_names_.push_back(port_type);
    }
  }

  // Get port configuration
  const std::vector<std::string>& get_port_types() const { return port_type_names_; }

  Bytes collect() override {
    // First collect base state
    Bytes bytes = Operator::collect();

    // Serialize data time tracker
    StateSerializer::serialize_port_timestamp_set_map(bytes, data_time_tracker_);

    // Serialize port type names
    StateSerializer::serialize_string_vector(bytes, port_type_names_);

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

    // Validate port types match
    if (port_type_names_.size() != num_data_ports()) {
      throw std::runtime_error("Port type count mismatch during restore");
    }
  }

  void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    auto time = msg->time;
    Operator::receive_data(std::move(msg), port_index);

    // Track timestamp
    data_time_tracker_[port_index].insert(time);
  }

 protected:
  void process_data() override {
    while (true) {
      // Find oldest common timestamp across all ports
      auto common_time = TimestampTracker::find_oldest_common_time(data_time_tracker_);
      if (!common_time) {
        break;
      }

      // Process messages with common timestamp
      bool all_ports_ready = true;
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
            get_output_queue(port).push_back(front_msg->clone());
            data_time_tracker_[port].erase(front_msg->time);
            queue.pop_front();
            message_found = true;
            break;
          } else {
            break;
          }
        }

        if (!message_found) {
          all_ports_ready = false;
          break;
        }
      }

      if (!all_ports_ready) {
        break;
      }
    }
  }

  void process_control() override {}  // Join has no control ports

 private:
  std::vector<std::string> port_type_names_;
  std::map<size_t, std::set<timestamp_t>> data_time_tracker_;
};

// Factory functions for common configurations
template <typename T>
inline std::unique_ptr<Join> make_binary_join(std::string id) {
  return std::make_unique<Join>(std::move(id), 2);
}

inline std::unique_ptr<Join> make_number_join(std::string id, size_t num_ports) {
  return std::make_unique<Join>(std::move(id), std::vector<std::string>(num_ports, PortType::NUMBER));
}

inline std::unique_ptr<Join> make_boolean_join(std::string id, size_t num_ports) {
  return std::make_unique<Join>(std::move(id), std::vector<std::string>(num_ports, PortType::BOOLEAN));
}

}  // namespace rtbot

#endif  // JOIN_H