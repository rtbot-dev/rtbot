#ifndef INPUT_H
#define INPUT_H

#include <string>
#include <unordered_map>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Input : public Operator {
 public:
  // Constructor takes a vector of port type strings
  Input(std::string id, const std::vector<std::string>& port_types) : Operator(std::move(id)) {
    for (const auto& type : port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown port type: " + type);
      }
      PortType::add_port(*this, type, true, true);
      last_sent_times_.push_back(0);
      port_type_names_.push_back(type);
    }
  }

  // Get port configuration
  const std::vector<std::string>& get_port_types() const { return port_type_names_; }

  // Query port state
  bool has_sent(size_t port_index) const {
    validate_port_index(port_index);
    return last_sent_times_[port_index] > 0;
  }

  timestamp_t get_last_sent_time(size_t port_index) const {
    validate_port_index(port_index);
    return last_sent_times_[port_index];
  }

  // State serialization
  Bytes collect() override {
    Bytes bytes;

    // Store number of ports
    const size_t num_ports = num_data_ports();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&num_ports),
                 reinterpret_cast<const uint8_t*>(&num_ports) + sizeof(num_ports));

    // Store last sent times
    for (const auto& time : last_sent_times_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                   reinterpret_cast<const uint8_t*>(&time) + sizeof(time));
    }

    // Store port types for validation
    for (const auto& type : port_type_names_) {
      size_t type_length = type.length();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&type_length),
                   reinterpret_cast<const uint8_t*>(&type_length) + sizeof(type_length));
      bytes.insert(bytes.end(), type.begin(), type.end());
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    // Read number of ports
    const size_t num_ports = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    // Validate port count
    if (num_ports != num_data_ports()) {
      throw std::runtime_error("Port count mismatch in restore");
    }

    // Read last sent times
    last_sent_times_.clear();
    last_sent_times_.reserve(num_ports);
    for (size_t i = 0; i < num_ports; ++i) {
      auto time = *reinterpret_cast<const timestamp_t*>(&(*it));
      it += sizeof(timestamp_t);
      last_sent_times_.push_back(time);
    }

    // Validate port types
    for (size_t i = 0; i < num_ports; ++i) {
      // Read port type string
      size_t type_length = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);
      std::string stored_type(it, it + type_length);
      it += type_length;

      if (stored_type != port_type_names_[i]) {
        throw std::runtime_error("Port type mismatch during restore for port " + std::to_string(i));
      }
    }
  }

 protected:
  void process_data() override {
    // Process each port that has new data
    for (const auto& port_index : data_ports_with_new_data_) {
      const auto& input_queue = get_data_queue(port_index);
      if (input_queue.empty()) continue;

      auto& output_queue = get_output_queue(port_index);

      // Process all messages in input queue
      for (const auto& msg : input_queue) {
        // Only forward if timestamp is increasing
        if (!has_sent(port_index) || msg->time > last_sent_times_[port_index]) {
          output_queue.push_back(msg->clone());
          last_sent_times_[port_index] = msg->time;
        }
      }

      // Clear processed messages
      get_data_queue(port_index).clear();
    }
  }

  void process_control() override {}  // No control processing needed

 private:
  void validate_port_index(size_t port_index) const {
    if (port_index >= num_data_ports()) {
      throw std::runtime_error("Invalid port index: " + std::to_string(port_index));
    }
  }

  std::vector<timestamp_t> last_sent_times_;
  std::vector<std::string> port_type_names_;
};

// Factory functions for common configurations
inline std::unique_ptr<Input> make_number_input(std::string id) {
  return std::make_unique<Input>(std::move(id), std::vector<std::string>{PortType::NUMBER});
}

inline std::unique_ptr<Input> make_boolean_input(std::string id) {
  return std::make_unique<Input>(std::move(id), std::vector<std::string>{PortType::BOOLEAN});
}

inline std::unique_ptr<Input> make_vector_number_input(std::string id) {
  return std::make_unique<Input>(std::move(id), std::vector<std::string>{PortType::VECTOR_NUMBER});
}

inline std::unique_ptr<Input> make_vector_boolean_input(std::string id) {
  return std::make_unique<Input>(std::move(id), std::vector<std::string>{PortType::VECTOR_BOOLEAN});
}

}  // namespace rtbot

#endif  // INPUT_H