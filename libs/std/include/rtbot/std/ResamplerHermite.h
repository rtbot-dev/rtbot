#ifndef RESAMPLER_HERMITE_H
#define RESAMPLER_HERMITE_H

#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/Message.h"

namespace rtbot {

struct ResamplerFeatures {
  static constexpr bool TRACK_SUM = false;
  static constexpr bool TRACK_MEAN = false;
  static constexpr bool TRACK_VARIANCE = false;
};

class ResamplerHermite : public Buffer<NumberData, ResamplerFeatures> {
 public:
  ResamplerHermite(std::string id, timestamp_t interval, std::optional<timestamp_t> t0 = std::nullopt)
      : Buffer<NumberData, ResamplerFeatures>(std::move(id), 4),
        dt_(interval),
        t0_(t0),
        next_emit_(0),
        initialized_(false) {
    if (interval <= 0) {
      throw std::runtime_error("Resampling interval must be positive");
    }
  }

  void reset() override {
    Buffer<NumberData, ResamplerFeatures>::reset();
    initialized_ = false;
    next_emit_ = 0;
    pending_emissions_.clear();
  }

  std::string type_name() const override { return "ResamplerHermite"; }
  Bytes collect() override {
    // First collect base state
    Bytes bytes = Buffer<NumberData, ResamplerFeatures>::collect();

    // Serialize next emission time
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&next_emit_),
                 reinterpret_cast<const uint8_t*>(&next_emit_) + sizeof(next_emit_));

    // Serialize initialization state
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&initialized_),
                 reinterpret_cast<const uint8_t*>(&initialized_) + sizeof(initialized_));

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    // First restore base state
    Buffer<NumberData, ResamplerFeatures>::restore(it);

    // Restore next emission time
    next_emit_ = *reinterpret_cast<const timestamp_t*>(&(*it));
    it += sizeof(timestamp_t);

    // Restore initialization state
    initialized_ = *reinterpret_cast<const bool*>(&(*it));
    it += sizeof(bool);
  }

  timestamp_t get_interval() const { return dt_; }
  timestamp_t get_next_emission_time() const { return next_emit_; }
  std::optional<timestamp_t> get_t0() const { return t0_; }

 protected:
  std::vector<std::unique_ptr<Message<NumberData>>> process_message(const Message<NumberData>* msg) override {
    if (!initialized_ && buffer_full()) {
      // Initialize next_emit_ based on t0_ or first complete buffer
      const auto& points = buffer();
      timestamp_t t1 = points[1]->time;  // Start from second point for proper interpolation
      if (t0_.has_value()) {
        if (t1 < *t0_) {
          next_emit_ = *t0_;
        } else {
          timestamp_t k = (t1 - *t0_) / dt_;
          next_emit_ = *t0_ + k * dt_;
        }
      } else {
        next_emit_ = t1;
      }
      initialized_ = true;
    }

    if (!initialized_) {
      return {};
    }

    const auto& points = buffer();
    std::vector<std::unique_ptr<Message<NumberData>>> emissions;

    // Continue emitting points as long as next_emit_ is within the interpolation window
    while (points[1]->time <= next_emit_ && next_emit_ <= points[2]->time) {
      double mu = static_cast<double>(next_emit_ - points[1]->time) / (points[2]->time - points[1]->time);

      double interpolated_value = hermite_interpolate(points[0]->data.value, points[1]->data.value,
                                                      points[2]->data.value, points[3]->data.value, mu);

      emissions.push_back(create_message<NumberData>(next_emit_, NumberData{interpolated_value}));
      next_emit_ += dt_;
    }

    return emissions;
  }

 private:
  static double hermite_interpolate(double y0, double y1, double y2, double y3, double mu, double tension = 0.0,
                                    double bias = 0.0) {
    // Calculate tangents
    double m0 = ((y1 - y0) * (1 + bias) * (1 - tension) / 2) + ((y2 - y1) * (1 - bias) * (1 - tension) / 2);
    double m1 = ((y2 - y1) * (1 + bias) * (1 - tension) / 2) + ((y3 - y2) * (1 - bias) * (1 - tension) / 2);

    // Hermite basis functions
    double mu2 = mu * mu;
    double mu3 = mu2 * mu;
    double h00 = 2 * mu3 - 3 * mu2 + 1;  // position term 1
    double h10 = mu3 - 2 * mu2 + mu;     // tangent term 1
    double h01 = -2 * mu3 + 3 * mu2;     // position term 2
    double h11 = mu3 - mu2;              // tangent term 2

    // Interpolate
    return h00 * y1 + h10 * m0 + h01 * y2 + h11 * m1;
  }

  timestamp_t dt_;                                                       // Resampling interval
  std::optional<timestamp_t> t0_;                                        // Optional start time
  timestamp_t next_emit_;                                                // Next time to emit a sample
  bool initialized_;                                                     // Whether we've initialized next_emit_
  std::vector<std::unique_ptr<Message<NumberData>>> pending_emissions_;  // Queue of pending emissions
};

inline std::shared_ptr<ResamplerHermite> make_resampler_hermite(std::string id, timestamp_t interval,
                                                                std::optional<timestamp_t> t0 = std::nullopt) {
  return std::make_shared<ResamplerHermite>(std::move(id), interval, t0);
}

}  // namespace rtbot

#endif  // RESAMPLER_HERMITE_H