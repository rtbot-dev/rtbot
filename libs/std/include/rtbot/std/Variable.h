#ifndef VARIABLE_H
#define VARIABLE_H

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "rtbot/Operator.h"
#include "rtbot/StateSerializer.h"

namespace rtbot {

class Variable : public Operator {
 public:
  Variable(std::string id, double default_value = 0.0)
      : Operator(std::move(id)), default_value_(default_value), initialized_(false) {
    // Single data port for value updates
    add_data_port<NumberData>();
    // Single control port for queries
    add_control_port<NumberData>();
    // Single output port for query responses
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "Variable"; }

  double get_default_value() const { return default_value_; }

  // State serialization
  Bytes collect() override {
    Bytes bytes = Operator::collect();

    // Serialize default value and initialization flag
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&default_value_),
                 reinterpret_cast<const uint8_t*>(&default_value_) + sizeof(default_value_));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&initialized_),
                 reinterpret_cast<const uint8_t*>(&initialized_) + sizeof(initialized_));

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    // Restore default value and initialization flag
    default_value_ = *reinterpret_cast<const double*>(&(*it));
    it += sizeof(double);
    initialized_ = *reinterpret_cast<const bool*>(&(*it));
    it += sizeof(bool);
  }

 protected:
  void process_data() override {
    initialized_ = true;
    // All data messages are already queued by the base class
  }

  void process_control() override {
    auto& control_queue = get_control_queue(0);
    auto& data_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!control_queue.empty()) {
      const auto* query = dynamic_cast<const Message<NumberData>*>(control_queue.front().get());
      if (!query) {
        throw std::runtime_error("Invalid control message type in Variable");
      }

      timestamp_t query_time = query->time;

      // If not initialized, return default value
      if (!initialized_ || data_queue.empty()) {
        output_queue.push_back(create_message<NumberData>(query_time, NumberData{default_value_}));
        control_queue.pop_front();
        continue;
      }

      // Find the last data point before or at query time
      double value = default_value_;
      bool found = false;

      for (const auto& msg_ptr : data_queue) {
        const auto* data_msg = dynamic_cast<const Message<NumberData>*>(msg_ptr.get());
        if (!data_msg) {
          throw std::runtime_error("Invalid data message type in Variable");
        }

        if (data_msg->time > query_time) {
          break;
        }

        value = data_msg->data.value;
        found = true;
      }

      if (found) {
        output_queue.push_back(create_message<NumberData>(query_time, NumberData{value}));
      } else {
        output_queue.push_back(create_message<NumberData>(query_time, NumberData{default_value_}));
      }

      control_queue.pop_front();
    }
  }

 private:
  double default_value_;
  bool initialized_;
};

// Factory function
inline std::unique_ptr<Variable> make_variable(std::string id, double default_value = 0.0) {
  return std::make_unique<Variable>(std::move(id), default_value);
}

}  // namespace rtbot

#endif  // VARIABLE_H