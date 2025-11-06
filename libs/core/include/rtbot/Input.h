#ifndef INPUT_H
#define INPUT_H

#include <string>
#include <unordered_map>
#include <vector>

#include "StateSerializer.h"
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

  std::string type_name() const override { return "Input"; }

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
    // First collect base state
    Bytes bytes = Operator::collect();

    // Serialize last sent times
    size_t num_ports = last_sent_times_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&num_ports),
                 reinterpret_cast<const uint8_t*>(&num_ports) + sizeof(num_ports));

    for (const auto& time : last_sent_times_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                   reinterpret_cast<const uint8_t*>(&time) + sizeof(time));
    }

    // Serialize port type names
    StateSerializer::serialize_string_vector(bytes, port_type_names_);

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    // First restore base state
    Operator::restore(it);

    // Restore last sent times
    size_t num_ports = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    StateSerializer::validate_port_count(num_ports, num_data_ports(), "Data");

    last_sent_times_.clear();
    last_sent_times_.reserve(num_ports);
    for (size_t i = 0; i < num_ports; ++i) {
      timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
      it += sizeof(timestamp_t);
      last_sent_times_.push_back(time);
    }

    // Restore port type names
    StateSerializer::deserialize_string_vector(it, port_type_names_);

    // Validate port types match
    if (port_type_names_.size() != num_data_ports()) {
      throw std::runtime_error("Port type count mismatch during restore");
    }
  }

  void reset() override {
    Operator::reset();
    last_sent_times_.assign(last_sent_times_.size(), 0);
  }

  // Do not throw exceptions in receive_data
  void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    try {
      Operator::receive_data(std::move(msg), port_index);      
    } catch (const std::exception& e) {
      
    }
  }

 protected:
  void process_data() override {
    // Process each port independently to allow concurrent timestamps
    for (const auto& port_index : data_ports_with_new_data_) {
      const auto& input_queue = get_data_queue(port_index);
      if (input_queue.empty()) continue;

      auto& output_queue = get_output_queue(port_index);

      // Process all messages in input queue
      for (const auto& msg : input_queue) {
        // Only forward if timestamp is increasing for this specific port
        if (!has_sent(port_index) || msg->time > last_sent_times_[port_index]) {
          output_queue.push_back(std::move(msg->clone()));
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
inline std::shared_ptr<Input> make_input(std::string id, const std::vector<std::string>& port_types) {
  return std::make_shared<Input>(std::move(id), port_types);
}

inline std::shared_ptr<Input> make_number_input(std::string id) {
  return std::make_shared<Input>(std::move(id), std::vector<std::string>{PortType::NUMBER});
}

inline std::shared_ptr<Input> make_boolean_input(std::string id) {
  return std::make_shared<Input>(std::move(id), std::vector<std::string>{PortType::BOOLEAN});
}

inline std::shared_ptr<Input> make_vector_number_input(std::string id) {
  return std::make_shared<Input>(std::move(id), std::vector<std::string>{PortType::VECTOR_NUMBER});
}

inline std::shared_ptr<Input> make_vector_boolean_input(std::string id) {
  return std::make_shared<Input>(std::move(id), std::vector<std::string>{PortType::VECTOR_BOOLEAN});
}

}  // namespace rtbot

#endif  // INPUT_H