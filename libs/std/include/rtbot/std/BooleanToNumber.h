#ifndef BOOLEAN_TO_NUMBER_H
#define BOOLEAN_TO_NUMBER_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

/// Converts BooleanData messages to NumberData messages.
/// true → 1.0, false → 0.0. Preserves timestamps.
class BooleanToNumber : public Operator {
 public:
  BooleanToNumber(std::string id) : Operator(std::move(id)) {
    add_data_port<BooleanData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "BooleanToNumber"; }

  bool equals(const BooleanToNumber& other) const {
    return Operator::equals(other);
  }

  bool operator==(const BooleanToNumber& other) const { return equals(other); }
  bool operator!=(const BooleanToNumber& other) const { return !(*this == other); }

 protected:
  void process_data(bool debug = false) override {
    auto& input_queue = get_data_queue(0);
    while (!input_queue.empty()) {
      const auto* msg =
          static_cast<const Message<BooleanData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in BooleanToNumber");
      }

      double value = msg->data.value ? 1.0 : 0.0;
      emit_output(0, create_message<NumberData>(msg->time, NumberData{value}), debug);
      input_queue.pop_front();
    }
  }
};

inline std::shared_ptr<BooleanToNumber> make_boolean_to_number(std::string id) {
  return std::make_shared<BooleanToNumber>(std::move(id));
}

}  // namespace rtbot

#endif  // BOOLEAN_TO_NUMBER_H
