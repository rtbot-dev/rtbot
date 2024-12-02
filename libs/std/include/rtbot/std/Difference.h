#ifndef DIFFERENCE_H
#define DIFFERENCE_H

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

// Difference only needs the buffer, no statistics tracking
struct DifferenceFeatures {
  static constexpr bool TRACK_SUM = false;
  static constexpr bool TRACK_MEAN = false;
  static constexpr bool TRACK_VARIANCE = false;
};

class Difference : public Buffer<NumberData, DifferenceFeatures> {
 public:
  Difference(std::string id, bool use_oldest_time = true)
      : Buffer<NumberData, DifferenceFeatures>(std::move(id), 2), use_oldest_time_(use_oldest_time) {}

  std::string type_name() const override { return "Difference"; }

  bool get_use_oldest_time() const { return use_oldest_time_; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData>* msg) override {
    std::vector<std::unique_ptr<Message<NumberData>>> output;

    // Need 2 points to compute difference
    if (!buffer_full()) {
      return output;
    }

    const auto& points = buffer();
    double diff_value = points[1]->data.value - points[0]->data.value;
    timestamp_t output_time = use_oldest_time_ ? points[1]->time : points[0]->time;

    output.push_back(create_message<NumberData>(output_time, NumberData{diff_value}));
    return output;
  }

 private:
  bool use_oldest_time_;
};

// Factory function for Difference
inline std::shared_ptr<Operator> make_difference(const std::string& id, bool use_oldest_time = true) {
  return std::make_shared<Difference>(id, use_oldest_time);
}

}  // namespace rtbot

#endif  // DIFFERENCE_H