#ifndef MOVING_VARIANCE_H
#define MOVING_VARIANCE_H

#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

struct MovingVarianceFeatures {
  static constexpr bool TRACK_SUM = false;
  static constexpr bool TRACK_VARIANCE = true;
};

class MovingVariance : public Buffer<NumberData, MovingVarianceFeatures> {
 public:
  MovingVariance(std::string id, size_t window_size)
      : Buffer<NumberData, MovingVarianceFeatures>(std::move(id), window_size) {}

  std::string type_name() const override { return "MovingVariance"; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData>* msg) override {
    if (!this->buffer_full()) {
      return {};
    }

    const double var = this->variance();
    std::vector<std::unique_ptr<Message<NumberData>>> output;
    output.push_back(create_message<NumberData>(msg->time, NumberData{var}));
    return output;
  }
};

inline std::shared_ptr<Operator> make_moving_variance(const std::string& id, size_t window_size) {
  return std::make_shared<MovingVariance>(id, window_size);
}

}  // namespace rtbot

#endif  // MOVING_VARIANCE_H
