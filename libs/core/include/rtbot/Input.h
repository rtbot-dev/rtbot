#ifndef INPUT_H
#define INPUT_H

#include <string>
#include <unordered_map>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class Input : public Operator {
 public:
  // Static port type string constants
  static constexpr const char* NUMBER_PORT = "number";
  static constexpr const char* BOOLEAN_PORT = "boolean";
  static constexpr const char* VECTOR_NUMBER_PORT = "vector_number";
  static constexpr const char* VECTOR_BOOLEAN_PORT = "vector_boolean";

  // Constructor takes a vector of port type strings
  Input(std::string id, const std::vector<std::string>& port_types) : Operator(std::move(id)) {
    for (const auto& type : port_types) {
      if (type == NUMBER_PORT) {
        add_number_port();
      } else if (type == BOOLEAN_PORT) {
        add_boolean_port();
      } else if (type == VECTOR_NUMBER_PORT) {
        add_vector_number_port();
      } else if (type == VECTOR_BOOLEAN_PORT) {
        add_vector_boolean_port();
      } else {
        throw std::runtime_error("Unknown port type: " + type);
      }
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

    // Store type information for validation
    for (size_t i = 0; i < num_ports; ++i) {
      const auto& type = get_data_port_type(i);
      const auto& type_hash = type.hash_code();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&type_hash),
                   reinterpret_cast<const uint8_t*>(&type_hash) + sizeof(type_hash));
    }

    return bytes;
  }

  void restore(std::vector<uint8_t>::const_iterator& it) override {
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

    // Validate type information
    for (size_t i = 0; i < num_ports; ++i) {
      const auto stored_type_hash = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);

      const auto& current_type = get_data_port_type(i);
      if (stored_type_hash != current_type.hash_code()) {
        throw std::runtime_error("Type mismatch during restore for port " + std::to_string(i));
      }
    }
  }

 protected:
  void process_data() override {
    // Process each port that has new data
    for (const auto& port_index : ports_with_new_data_) {
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
  void add_number_port() {
    add_data_port<NumberData>();
    add_output_port<NumberData>();
    last_sent_times_.push_back(0);
  }

  void add_boolean_port() {
    add_data_port<BooleanData>();
    add_output_port<BooleanData>();
    last_sent_times_.push_back(0);
  }

  void add_vector_number_port() {
    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();
    last_sent_times_.push_back(0);
  }

  void add_vector_boolean_port() {
    add_data_port<VectorBooleanData>();
    add_output_port<VectorBooleanData>();
    last_sent_times_.push_back(0);
  }

  void validate_port_index(size_t port_index) const {
    if (port_index >= num_data_ports()) {
      throw std::runtime_error("Invalid port index: " + std::to_string(port_index));
    }
  }

  std::vector<timestamp_t> last_sent_times_;
  std::vector<std::string> port_type_names_;  // Store port type names for serialization
};

// Factory functions for common configurations
inline std::unique_ptr<Input> make_number_input(std::string id) {
  return std::make_unique<Input>(std::move(id), std::vector<std::string>{Input::NUMBER_PORT});
}

inline std::unique_ptr<Input> make_boolean_input(std::string id) {
  return std::make_unique<Input>(std::move(id), std::vector<std::string>{Input::BOOLEAN_PORT});
}

inline std::unique_ptr<Input> make_vector_number_input(std::string id) {
  return std::make_unique<Input>(std::move(id), std::vector<std::string>{Input::VECTOR_NUMBER_PORT});
}

inline std::unique_ptr<Input> make_vector_boolean_input(std::string id) {
  return std::make_unique<Input>(std::move(id), std::vector<std::string>{Input::VECTOR_BOOLEAN_PORT});
}

}  // namespace rtbot

#endif  // INPUT_H