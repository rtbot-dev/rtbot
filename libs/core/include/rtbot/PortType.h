#ifndef PORT_TYPE_H
#define PORT_TYPE_H

#include <stdexcept>
#include <string>
#include <typeindex>

#include "rtbot/Message.h"

namespace rtbot {

class PortType {
 public:
  // Port type string constants
  static constexpr const char* NUMBER = "number";
  static constexpr const char* BOOLEAN = "boolean";
  static constexpr const char* VECTOR_NUMBER = "vector_number";
  static constexpr const char* VECTOR_BOOLEAN = "vector_boolean";

  // Get the corresponding data type for a port type string
  static std::type_index get_data_type(const std::string& port_type) {
    if (port_type == NUMBER) {
      return std::type_index(typeid(NumberData));
    } else if (port_type == BOOLEAN) {
      return std::type_index(typeid(BooleanData));
    } else if (port_type == VECTOR_NUMBER) {
      return std::type_index(typeid(VectorNumberData));
    } else if (port_type == VECTOR_BOOLEAN) {
      return std::type_index(typeid(VectorBooleanData));
    } else {
      throw std::runtime_error("Unknown port type: " + port_type);
    }
  }

  // Get port type string from data type
  template <typename T>
  static std::string get_port_type() {
    if constexpr (std::is_same_v<T, NumberData>) {
      return NUMBER;
    } else if constexpr (std::is_same_v<T, BooleanData>) {
      return BOOLEAN;
    } else if constexpr (std::is_same_v<T, VectorNumberData>) {
      return VECTOR_NUMBER;
    } else if constexpr (std::is_same_v<T, VectorBooleanData>) {
      return VECTOR_BOOLEAN;
    } else {
      throw std::runtime_error("Unsupported data type");
    }
  }

  // Validate port type string
  static bool is_valid_port_type(const std::string& port_type) {
    return port_type == NUMBER || port_type == BOOLEAN || port_type == VECTOR_NUMBER || port_type == VECTOR_BOOLEAN;
  }

  // Create appropriate message for port type
  static std::unique_ptr<BaseMessage> create_message(const std::string& port_type, timestamp_t time) {
    if (port_type == NUMBER) {
      return rtbot::create_message<NumberData>(time, {0.0});
    } else if (port_type == BOOLEAN) {
      return rtbot::create_message<BooleanData>(time, {false});
    } else if (port_type == VECTOR_NUMBER) {
      return rtbot::create_message<VectorNumberData>(time, {{}});
    } else if (port_type == VECTOR_BOOLEAN) {
      return rtbot::create_message<VectorBooleanData>(time, {{}});
    } else {
      throw std::runtime_error("Unknown port type: " + port_type);
    }
  }

  // Helper method to add port of specified type to an operator
  template <typename OperatorType>
  static void add_port(OperatorType& op, const std::string& port_type, bool is_data = true, bool is_control = false,
                       bool add_output = false) {
    if (port_type == NUMBER) {
      if (is_data)
        op.template add_data_port<NumberData>();
      else if (is_control)
        op.template add_control_port<NumberData>();
      if (add_output) op.template add_output_port<NumberData>();
    } else if (port_type == BOOLEAN) {
      if (is_data)
        op.template add_data_port<BooleanData>();
      else if (is_control)
        op.template add_control_port<BooleanData>();
      if (add_output) op.template add_output_port<BooleanData>();
    } else if (port_type == VECTOR_NUMBER) {
      if (is_data)
        op.template add_data_port<VectorNumberData>();
      else if (is_control)
        op.template add_control_port<VectorNumberData>();
      if (add_output) op.template add_output_port<VectorNumberData>();
    } else if (port_type == VECTOR_BOOLEAN) {
      if (is_data)
        op.template add_data_port<VectorBooleanData>();
      else if (is_control)
        op.template add_control_port<VectorBooleanData>();
      if (add_output) op.template add_output_port<VectorBooleanData>();
    } else {
      throw std::runtime_error("Unknown port type: " + port_type);
    }
  }
};

}  // namespace rtbot

#endif  // PORT_TYPE_H
