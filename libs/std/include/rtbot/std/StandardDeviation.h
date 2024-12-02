#ifndef STANDARD_DEVIATION_H
#define STANDARD_DEVIATION_H

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

// StandardDeviation needs mean and variance tracking
struct StandardDeviationFeatures {
  static constexpr bool TRACK_SUM = false;      // Not needed directly
  static constexpr bool TRACK_MEAN = true;      // Required for variance
  static constexpr bool TRACK_VARIANCE = true;  // Required for std dev
};

class StandardDeviation : public Buffer<NumberData, StandardDeviationFeatures> {
 public:
  StandardDeviation(std::string id, size_t window_size)
      : Buffer<NumberData, StandardDeviationFeatures>(std::move(id), window_size) {}

  std::string type_name() const override { return "StandardDeviation"; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData> *msg) override {
    // Only emit messages when the buffer is full to ensure
    // we have enough data for a proper standard deviation
    if (!this->buffer_full()) {
      return {};
    }

    // Create output message with same timestamp but standard deviation value
    std::vector<std::unique_ptr<Message<NumberData>>> v;
    v.push_back(create_message<NumberData>(msg->time, NumberData{this->standard_deviation()}));
    return v;
  }
};

inline std::shared_ptr<Operator> make_std_dev(const std::string &id, size_t window_size) {
  return std::make_shared<StandardDeviation>(id, window_size);
}

}  // namespace rtbot

#endif  // STANDARD_DEVIATION_H