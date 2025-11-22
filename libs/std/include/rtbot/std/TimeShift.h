#ifndef TIME_SHIFT_H
#define TIME_SHIFT_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class TimeShift : public Operator {
 public:
  TimeShift(std::string id, timestamp_t shift) : Operator(std::move(id)), shift_(shift) {
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "TimeShift"; }
  timestamp_t get_shift() const { return shift_; }

  bool equals(const TimeShift& other) const {
    return (shift_ == other.shift_ && Operator::equals(other));
  }

  bool operator==(const TimeShift& other) const {
    return equals(other);
  }

  bool operator!=(const TimeShift& other) const {
    return !(*this == other);
  }

 protected:
  void process_data(bool debug=false) override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in TimeShift");
      }

      // Create output message with shifted time
      timestamp_t new_time = msg->time + shift_;
      // Only emit if the resulting time would be non-negative
      if (new_time >= 0) {
        output_queue.push_back(create_message<NumberData>(new_time, msg->data));
      } else {
        throw std::runtime_error("Negative new time " + std::to_string(new_time) + " in TimeShift");
      }

      input_queue.pop_front();
    }
  }

 private:
  timestamp_t shift_;
};

// Factory function
inline std::unique_ptr<TimeShift> make_time_shift(std::string id, timestamp_t shift) {
  return std::make_unique<TimeShift>(std::move(id), shift);
}

}  // namespace rtbot

#endif  // TIME_SHIFT_H