#ifndef MOVING_SUM_H
#define MOVING_SUM_H

#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

// MovingSum only needs sum tracking
struct MovingSumFeatures {
  static constexpr bool TRACK_SUM = true;
  static constexpr bool TRACK_VARIANCE = false;
};

class MovingSum : public Buffer<NumberData, MovingSumFeatures> {
 public:
  MovingSum(std::string id, size_t window_size) : Buffer<NumberData, MovingSumFeatures>(std::move(id), window_size) {}

  std::string type_name() const override { return "MovingSum"; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData> *msg) override {
    // Only emit messages when the buffer is full to ensure
    // we have enough data for a proper moving sum
    if (!this->buffer_full()) {
      return {};
    }

    // Create output message with same timestamp but sum value
    std::vector<std::unique_ptr<Message<NumberData>>> v;
    v.push_back(create_message<NumberData>(msg->time, NumberData{this->sum()}));
    return v;
  }
};

inline std::shared_ptr<MovingSum> make_moving_sum(std::string id, size_t window_size) {
  return std::make_shared<MovingSum>(std::move(id), window_size);
}

}  // namespace rtbot

#endif  // MOVING_SUM_H