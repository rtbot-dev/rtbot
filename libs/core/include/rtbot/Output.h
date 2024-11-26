#ifndef OUTPUT_H
#define OUTPUT_H

#include "Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Output : public Operator {
 public:
  Output(std::string id, const std::vector<std::string>& port_types) : Operator(std::move(id)) {
    if (port_types.empty()) {
      throw std::runtime_error("Output operator must have at least one port type");
    }

    // Create corresponding input and output ports
    for (const auto& type : port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Invalid port type: " + type);
      }

      // Store port type
      port_type_names_.push_back(type);

      // Add input port and matching output port
      PortType::add_port(*this, type, true, true);  // input port
    }
  }

  std::string type_name() const override { return "Output"; }

  // Get port configuration
  const std::vector<std::string>& get_port_types() const { return port_type_names_; }

 protected:
  void process_data() override {
    // Forward all messages from inputs to corresponding outputs
    for (size_t i = 0; i < num_data_ports(); ++i) {
      auto& input_queue = get_data_queue(i);
      auto& output_queue = get_output_queue(i);

      // Forward all messages
      while (!input_queue.empty()) {
        output_queue.push_back(input_queue.front()->clone());
        input_queue.pop_front();
      }
    }
  }

 private:
  std::vector<std::string> port_type_names_;
};

// Factory functions for common configurations
inline std::unique_ptr<Output> make_number_output(std::string id) {
  return std::make_unique<Output>(std::move(id), std::vector<std::string>{PortType::NUMBER});
}

inline std::unique_ptr<Output> make_boolean_output(std::string id) {
  return std::make_unique<Output>(std::move(id), std::vector<std::string>{PortType::BOOLEAN});
}

inline std::unique_ptr<Output> make_vector_number_output(std::string id) {
  return std::make_unique<Output>(std::move(id), std::vector<std::string>{PortType::VECTOR_NUMBER});
}

inline std::unique_ptr<Output> make_vector_boolean_output(std::string id) {
  return std::make_unique<Output>(std::move(id), std::vector<std::string>{PortType::VECTOR_BOOLEAN});
}

}  // namespace rtbot

#endif  // OUTPUT_H