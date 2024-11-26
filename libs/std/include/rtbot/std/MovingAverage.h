#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

// MovingAverage only needs sum and mean tracking
struct MovingAverageFeatures {
  static constexpr bool TRACK_SUM = true;
  static constexpr bool TRACK_MEAN = true;
  static constexpr bool TRACK_VARIANCE = false;
};

class MovingAverage : public Buffer<NumberData, MovingAverageFeatures> {
 public:
  MovingAverage(std::string id, size_t window_size)
      : Buffer<NumberData, MovingAverageFeatures>(std::move(id), window_size) {}

  std::string type_name() const override { return "MovingAverage"; }

 protected:
  std::unique_ptr<Message<NumberData>> process_message(const Message<NumberData>* msg) override {
    // Only emit messages when the buffer is full to ensure
    // we have enough data for a proper moving average
    if (!this->buffer_full()) {
      return nullptr;
    }

    // Create output message with same timestamp but mean value
    return create_message<NumberData>(msg->time, NumberData{this->mean()});
  }
};

}  // namespace rtbot

#endif  // MOVING_AVERAGE_H