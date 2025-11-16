#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

// MovingAverage only needs sum and mean tracking
struct MovingAverageFeatures {
  static constexpr bool TRACK_SUM = true;
  static constexpr bool TRACK_VARIANCE = false;
};

class MovingAverage : public Buffer<NumberData, MovingAverageFeatures> {
 public:
  MovingAverage(std::string id, size_t window_size)
      : Buffer<NumberData, MovingAverageFeatures>(std::move(id), window_size) {}

  std::string type_name() const override { return "MovingAverage"; }

  bool equals(const MovingAverage& other) const {
    return (StateSerializer::hash_double(mean()) == StateSerializer::hash_double(other.mean()) && Buffer<NumberData, MovingAverageFeatures>::equals(other));
  }

  bool operator==(const MovingAverage& other) const {
    return equals(other);
  }

  bool operator!=(const MovingAverage& other) const {
      return !(*this == other);
  }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData> *msg) override {
    // Only emit messages when the buffer is full to ensure
    // we have enough data for a proper moving average
    if (!this->buffer_full()) {
      return {};
    }

    // Create output message with same timestamp but mean value
    std::vector<std::unique_ptr<Message<NumberData>>> v;
    v.push_back(create_message<NumberData>(msg->time, NumberData{this->mean()}));
    return v;
  }
};

inline std::shared_ptr<MovingAverage> make_moving_average(std::string id, size_t window_size) {
  return std::make_shared<MovingAverage>(std::move(id), window_size);
}

}  // namespace rtbot

#endif  // MOVING_AVERAGE_H