#ifndef TIMESTAMP_EXTRACT_H
#define TIMESTAMP_EXTRACT_H

#include <memory>
#include <string>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

// Extracts the message timestamp as a NumberData value.
// Input: VectorNumberData (same as VectorExtract)
// Output: NumberData containing static_cast<double>(msg->time)
class TimestampExtract : public Operator {
 public:
  TimestampExtract(std::string id) : Operator(std::move(id)) {
    add_data_port<VectorNumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "TimestampExtract"; }

  bool equals(const TimestampExtract& other) const {
    return Operator::equals(other);
  }

  bool operator==(const TimestampExtract& other) const { return equals(other); }
  bool operator!=(const TimestampExtract& other) const { return !(*this == other); }

 protected:
  void process_data(bool debug = false) override {
    auto& input_queue = get_data_queue(0);
    while (!input_queue.empty()) {
      const auto* msg = static_cast<const Message<VectorNumberData>*>(
          input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in TimestampExtract");
      }
      emit_output(0,
          create_message<NumberData>(msg->time, NumberData{static_cast<double>(msg->time)}), debug);
      input_queue.pop_front();
    }
  }
};

inline std::shared_ptr<TimestampExtract> make_timestamp_extract(std::string id) {
  return std::make_shared<TimestampExtract>(std::move(id));
}

}  // namespace rtbot

#endif  // TIMESTAMP_EXTRACT_H
