#ifndef RELATIVESTRENGTHINDEX_H
#define RELATIVESTRENGTHINDEX_H

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

// RSI needs to track sum for computing averages
struct RSIFeatures {
  static constexpr bool TRACK_SUM = true;
  static constexpr bool TRACK_VARIANCE = false;
};

class RelativeStrengthIndex : public Buffer<NumberData, RSIFeatures> {
 public:
  RelativeStrengthIndex(std::string id, size_t n)
      : Buffer<NumberData, RSIFeatures>(std::move(id), n + 1),
        initialized_(false),
        average_gain_(0.0),
        average_loss_(0.0),
        prev_average_gain_(0.0),
        prev_average_loss_(0.0) {}

  std::string type_name() const override { return "RelativeStrengthIndex"; }

  void reset() override {
    Buffer<NumberData, RSIFeatures>::reset();
    initialized_ = false;
    average_gain_ = 0.0;
    average_loss_ = 0.0;
    prev_average_gain_ = 0.0;
    prev_average_loss_ = 0.0;
  }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData>* msg) override {
    // Only compute RSI when buffer is full
    if (!buffer_full()) {
      return {};
    }

    size_t n = buffer_size();
    double diff, rs, rsi, gain, loss;

    if (!initialized_) {
      average_gain_ = 0.0;
      average_loss_ = 0.0;

      // Calculate initial average gain/loss from buffer
      for (size_t i = 1; i < n; i++) {
        diff = buffer()[i]->data.value - buffer()[i - 1]->data.value;
        if (diff > 0) {
          average_gain_ += diff;
        } else if (diff < 0) {
          average_loss_ -= diff;  // Make positive
        }
      }
      average_gain_ /= (n - 1);
      average_loss_ /= (n - 1);

      initialized_ = true;
    } else {
      // Use smoothed average for subsequent calculations
      diff = buffer()[n - 1]->data.value - buffer()[n - 2]->data.value;
      if (diff > 0) {
        gain = diff;
        loss = 0.0;
      } else if (diff < 0) {
        loss = -diff;
        gain = 0.0;
      } else {
        loss = 0.0;
        gain = 0.0;
      }
      average_gain_ = (prev_average_gain_ * (n - 2) + gain) / (n - 1);
      average_loss_ = (prev_average_loss_ * (n - 2) + loss) / (n - 1);
    }

    prev_average_gain_ = average_gain_;
    prev_average_loss_ = average_loss_;

    // Calculate RSI
    if (average_loss_ > 0) {
      rs = average_gain_ / average_loss_;
      rsi = 100.0 - (100.0 / (1.0 + rs));
    } else {
      rsi = 100.0;
    }

    // Create output message
    std::vector<std::unique_ptr<Message<NumberData>>> result;
    result.push_back(create_message<NumberData>(msg->time, NumberData{rsi}));
    return result;
  }

 private:
  bool initialized_;
  double average_gain_;
  double average_loss_;
  double prev_average_gain_;
  double prev_average_loss_;
};

inline std::shared_ptr<RelativeStrengthIndex> make_relative_strength_index(std::string id, size_t n) {
  return std::make_shared<RelativeStrengthIndex>(std::move(id), n);
}

}  // namespace rtbot

#endif  // RELATIVESTRENGTHINDEX_H
