#ifndef PEAK_DETECTOR_H
#define PEAK_DETECTOR_H

#include <cstddef>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

// PeakDetector only needs buffer functionality, no statistics
struct PeakDetectorFeatures {
  static constexpr bool TRACK_SUM = false;
  static constexpr bool TRACK_MEAN = false;
  static constexpr bool TRACK_VARIANCE = false;
};

class PeakDetector : public Buffer<NumberData, PeakDetectorFeatures> {
 public:
  PeakDetector(std::string id, size_t window_size)
      : Buffer<NumberData, PeakDetectorFeatures>(std::move(id), window_size) {
    if (window_size < 3) {
      throw std::runtime_error("PeakDetector window size must be at least 3");
    }
    if (window_size % 2 == 0) {
      throw std::runtime_error("PeakDetector window size must be odd");
    }
  }

  std::string type_name() const override { return "PeakDetector"; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData>* msg) override {
    std::vector<std::unique_ptr<Message<NumberData>>> output;

    // Only process when buffer is full
    if (!buffer_full()) {
      return output;
    }

    const auto& buf = buffer();
    size_t window_size_ = window_size();
    size_t center = window_size_ / 2;  // Integer division gives us the center position

    // Check if center point is a local maximum
    bool is_peak = true;
    double center_value = buf[center]->data.value;

    for (size_t i = 0; i < window_size_; i++) {
      if (i != center && buf[i]->data.value >= center_value) {
        is_peak = false;
        break;
      }
    }

    // If it's a peak, emit the center point
    if (is_peak) {
      output.push_back(create_message<NumberData>(buf[center]->time, buf[center]->data));
    }

    return output;
  }
};

}  // namespace rtbot

#endif  // PEAK_DETECTOR_H