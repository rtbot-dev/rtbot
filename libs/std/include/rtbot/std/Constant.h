#ifndef CONSTANT_H
#define CONSTANT_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

template <typename T>
class Constant : public Operator {
 public:
  Constant(std::string id, const T& value) : Operator(std::move(id)), value_(value) {
    // Add single data input port and output port
    add_data_port<T>();
    add_output_port<T>();
  }

  std::string type_name() const override { return "Constant"; }

  // Accessor for the constant value
  const T& get_value() const { return value_; }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<T>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Constant");
      }

      // Create output message with same timestamp but constant value
      output_queue.push_back(create_message<T>(msg->time, value_));
      input_queue.pop_front();
    }
  }

 private:
  T value_;  // The constant value to emit
};

// Factory functions for common configurations
inline std::unique_ptr<Constant<NumberData>> make_number_constant(std::string id, double value) {
  return std::make_unique<Constant<NumberData>>(std::move(id), NumberData{value});
}

inline std::unique_ptr<Constant<BooleanData>> make_boolean_constant(std::string id, bool value) {
  return std::make_unique<Constant<BooleanData>>(std::move(id), BooleanData{value});
}

}  // namespace rtbot

#endif  // CONSTANT_H