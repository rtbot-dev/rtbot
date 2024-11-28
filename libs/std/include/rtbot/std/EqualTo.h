#ifndef EQUAL_TO_H
#define EQUAL_TO_H

#include <cmath>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class EqualTo : public Operator {
 public:
  EqualTo(std::string id, double value, double tolerance = 0.0)
      : Operator(std::move(id)), value_(value), tolerance_(std::abs(tolerance)) {
    // Add single input and output port
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "EqualTo"; }

  double get_value() const { return value_; }
  double get_tolerance() const { return tolerance_; }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in EqualTo");
      }

      if (std::abs(msg->data.value - value_) <= tolerance_) {
        output_queue.push_back(input_queue.front()->clone());
      }

      input_queue.pop_front();
    }
  }

 private:
  double value_;
  double tolerance_;
};

}  // namespace rtbot

#endif  // EQUAL_TO_H