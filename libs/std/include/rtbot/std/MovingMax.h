#ifndef MOVING_MAX_H
#define MOVING_MAX_H

#include <algorithm>
#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

struct MovingMaxFeatures {
  static constexpr bool TRACK_SUM = false;
  static constexpr bool TRACK_VARIANCE = false;
};

class MovingMax : public Buffer<NumberData, MovingMaxFeatures> {
 public:
  MovingMax(std::string id, size_t window_size)
      : Buffer<NumberData, MovingMaxFeatures>(std::move(id), window_size) {}

  std::string type_name() const override { return "MovingMax"; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData>* msg) override {
    if (!this->buffer_full()) {
      return {};
    }

    double max_value = this->buffer().front()->data.value;
    for (const auto& entry : this->buffer()) {
      max_value = std::max(max_value, entry->data.value);
    }

    std::vector<std::unique_ptr<Message<NumberData>>> output;
    output.push_back(create_message<NumberData>(msg->time, NumberData{max_value}));
    return output;
  }
};

inline std::shared_ptr<Operator> make_moving_max(const std::string& id, size_t window_size) {
  return std::make_shared<MovingMax>(id, window_size);
}

}  // namespace rtbot

#endif  // MOVING_MAX_H
