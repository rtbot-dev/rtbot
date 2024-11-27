#ifndef ADD_H
#define ADD_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Add : public Operator {
 public:
  Add(std::string id, double value) : Operator(std::move(id)), value_(value) {
    // Add single input port for numbers
    add_data_port<NumberData>();

    // Add single output port
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "Add"; }

  // Get the constant value being added
  double get_value() const { return value_; }

  // State serialization
  Bytes collect() override {
    Bytes bytes = Operator::collect();

    // Serialize the value
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&value_),
                 reinterpret_cast<const uint8_t*>(&value_) + sizeof(value_));

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    // First restore base state
    Operator::restore(it);

    // Restore value
    value_ = *reinterpret_cast<const double*>(&(*it));
    it += sizeof(double);
  }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Add");
      }

      // Create output message with same timestamp but added value
      output_queue.push_back(create_message<NumberData>(msg->time, NumberData{msg->data.value + value_}));

      input_queue.pop_front();
    }
  }

 private:
  double value_;  // The constant value to add
};

// Factory function
inline std::unique_ptr<Add> make_add(std::string id, double value) {
  return std::make_unique<Add>(std::move(id), value);
}

}  // namespace rtbot

#endif  // ADD_H