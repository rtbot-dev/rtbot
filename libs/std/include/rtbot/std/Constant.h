#ifndef CONSTANT_H
#define CONSTANT_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

template <typename InputT, typename OutputT = InputT>
class Constant : public Operator {
 public:
  Constant(std::string id, const OutputT& value) : Operator(std::move(id)), value_(value) {
    // Add input and output ports with potentially different types
    add_data_port<InputT>();
    add_output_port<OutputT>();
  }

  std::string type_name() const override { return "Constant"; }

  // Accessor for the constant value
  const OutputT& get_value() const { return value_; }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<InputT>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Constant");
      }

      // Create output message with same timestamp but constant value
      output_queue.push_back(create_message<OutputT>(msg->time, value_));
      input_queue.pop_front();
    }
  }

 private:
  OutputT value_;  // The constant value to emit
};

// Type-specific aliases for common configurations
using ConstantNumber = Constant<NumberData>;
using ConstantBoolean = Constant<BooleanData>;
using ConstantNumberToBoolean = Constant<NumberData, BooleanData>;
using ConstantBooleanToNumber = Constant<BooleanData, NumberData>;

// Factory functions for same-type constants
inline std::shared_ptr<ConstantNumber> make_constant_number(std::string id, double value) {
  return std::make_shared<ConstantNumber>(std::move(id), NumberData{value});
}

inline std::shared_ptr<ConstantBoolean> make_constant_boolean(std::string id, bool value) {
  return std::make_shared<ConstantBoolean>(std::move(id), BooleanData{value});
}

// Factory functions for type-converting constants
inline std::shared_ptr<ConstantNumberToBoolean> make_constant_number_to_boolean(std::string id, bool value) {
  return std::make_shared<ConstantNumberToBoolean>(std::move(id), BooleanData{value});
}

inline std::shared_ptr<ConstantBooleanToNumber> make_constant_boolean_to_number(std::string id, double value) {
  return std::make_shared<ConstantBooleanToNumber>(std::move(id), NumberData{value});
}

// Vector constant types
using ConstantVectorNumber = Constant<VectorNumberData>;
using ConstantVectorBoolean = Constant<VectorBooleanData>;
using ConstantNumberToVectorNumber = Constant<NumberData, VectorNumberData>;
using ConstantBooleanToVectorBoolean = Constant<BooleanData, VectorBooleanData>;
using ConstantVectorNumberToNumber = Constant<VectorNumberData, NumberData>;
using ConstantVectorBooleanToBoolean = Constant<VectorBooleanData, BooleanData>;

// Factory functions for vector constants
inline std::shared_ptr<ConstantVectorNumber> make_constant_vector_number(std::string id,
                                                                         const std::vector<double>& value) {
  return std::make_shared<ConstantVectorNumber>(std::move(id), VectorNumberData{value});
}

inline std::shared_ptr<ConstantVectorBoolean> make_constant_vector_boolean(std::string id,
                                                                           const std::vector<bool>& value) {
  return std::make_shared<ConstantVectorBoolean>(std::move(id), VectorBooleanData{value});
}

// Factory functions for scalar-to-vector constants
inline std::shared_ptr<ConstantNumberToVectorNumber> make_constant_number_to_vector_number(
    std::string id, const std::vector<double>& value) {
  return std::make_shared<ConstantNumberToVectorNumber>(std::move(id), VectorNumberData{value});
}

inline std::shared_ptr<ConstantBooleanToVectorBoolean> make_constant_boolean_to_vector_boolean(
    std::string id, const std::vector<bool>& value) {
  return std::make_shared<ConstantBooleanToVectorBoolean>(std::move(id), VectorBooleanData{value});
}

// Factory functions for vector-to-scalar constants
inline std::shared_ptr<ConstantVectorNumberToNumber> make_constant_vector_number_to_number(std::string id,
                                                                                           double value) {
  return std::make_shared<ConstantVectorNumberToNumber>(std::move(id), NumberData{value});
}

inline std::shared_ptr<ConstantVectorBooleanToBoolean> make_constant_vector_boolean_to_boolean(std::string id,
                                                                                               bool value) {
  return std::make_shared<ConstantVectorBooleanToBoolean>(std::move(id), BooleanData{value});
}

}  // namespace rtbot

#endif  // CONSTANT_H