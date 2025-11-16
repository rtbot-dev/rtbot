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
      PortType::add_port(*this, type, true, false ,true);
      port_type_names_.push_back(type);
    }
  }

  std::string type_name() const override { return "Input"; }

  // Get port configuration
  const std::vector<std::string>& get_port_types() const { return port_type_names_; }

  bool equals(const Input& other) const {
    if (port_type_names_ != other.port_type_names_) return false;
    return Operator::equals(other);
  }
  
  bool operator==(const Input& other) const {
    return equals(other);
  }

  bool operator!=(const Input& other) const {
    return !(*this == other);
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
    for (int port_index = 0; port_index < num_data_ports(); port_index++) {
      const auto& input_queue = get_data_queue(port_index);
      if (input_queue.empty()) continue;

      auto& output_queue = get_output_queue(port_index);

      // Process all messages in input queue
      for (const auto& msg : input_queue) {        
          output_queue.push_back(std::move(msg->clone()));       
      }

      // Clear processed messages
      get_data_queue(port_index).clear();
    }
  }


 private:
  void validate_port_index(size_t port_index) const {
    if (port_index >= num_data_ports()) {
      throw std::runtime_error("Invalid port index: " + std::to_string(port_index));
    }
  }
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