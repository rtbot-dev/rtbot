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

  // Input is the system boundary — ignore the caller's debug flag and always
  // enforce timestamp ordering on ingress. Downstream operators trust their
  // producers, so the base check is debug-gated; Input's is not.
  // Exceptions are swallowed so invalid ingress messages are silently dropped.
  void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index, bool /*debug*/ = false) override {
    try {
      Operator::receive_data(std::move(msg), port_index, /*debug=*/true);
    } catch (const std::exception& e) {
      // Do nothing
    }
  }

  void receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index, bool /*debug*/ = false) override {
    try {
      Operator::receive_control(std::move(msg), port_index, /*debug=*/true);
    } catch (const std::exception& e) {
      // Do nothing
    }
  }

  bool uses_base_receive_data() const override { return false; }

 protected:
  void process_data(bool debug=false) override {
    // Process each port independently to allow concurrent timestamps
    for (int port_index = 0; port_index < num_data_ports(); port_index++) {
      auto& input_queue = get_data_queue(port_index);
      while (!input_queue.empty()) {
        emit_output(port_index, std::move(input_queue.front()), debug);
        input_queue.pop_front();
      }
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
