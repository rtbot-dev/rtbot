#ifndef MOVING_MIN_H
#define MOVING_MIN_H

#include <algorithm>
#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

struct MovingMinFeatures {
  static constexpr bool TRACK_SUM = false;
  static constexpr bool TRACK_VARIANCE = false;
};

class MovingMin : public Buffer<NumberData, MovingMinFeatures> {
 public:
  MovingMin(std::string id, size_t window_size)
      : Buffer<NumberData, MovingMinFeatures>(std::move(id), window_size) {}

  std::string type_name() const override { return "MovingMin"; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData>* msg) override {
    if (!this->buffer_full()) {
      return {};
    }

    double min_value = this->buffer().front()->data.value;
    for (const auto& entry : this->buffer()) {
      min_value = std::min(min_value, entry->data.value);
    }

    std::vector<std::unique_ptr<Message<NumberData>>> output;
    output.push_back(create_message<NumberData>(msg->time, NumberData{min_value}));
    return output;
  }
};

inline std::shared_ptr<Operator> make_moving_min(const std::string& id, size_t window_size) {
  return std::make_shared<MovingMin>(id, window_size);
}

}  // namespace rtbot

#endif  // MOVING_MIN_H
