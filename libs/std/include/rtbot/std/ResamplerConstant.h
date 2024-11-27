#ifndef RESAMPLER_CONSTANT_H
#define RESAMPLER_CONSTANT_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

template <typename T>
class ResamplerConstant : public Operator {
 public:
  explicit ResamplerConstant(std::string id, timestamp_t interval, std::optional<timestamp_t> t0 = std::nullopt)
      : Operator(std::move(id)), dt_(interval), t0_(t0), next_emit_(0), initialized_(false) {
    if (interval <= 0) {
      throw std::runtime_error("Time interval must be positive");
    }

    add_data_port<T>();
    add_output_port<T>();
  }

  std::string type_name() const override { return "ResamplerConstant"; }

  timestamp_t get_interval() const { return dt_; }
  timestamp_t get_next_emission_time() const { return next_emit_; }
  std::optional<timestamp_t> get_t0() const { return t0_; }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<T>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in ResamplerConstant");
      }

      // Initialize on first message
      if (!initialized_) {
        if (t0_.has_value()) {
          // Find next grid point after msg->time based on t0
          if (msg->time < *t0_) {
            next_emit_ = *t0_;
          } else {
            timestamp_t k = (msg->time - *t0_) / dt_;  // integer division
            next_emit_ = *t0_ + (k + 1) * dt_;
          }
        } else {
          next_emit_ = msg->time + dt_;
        }
        last_value_ = msg->data;
        initialized_ = true;
        input_queue.pop_front();
        continue;
      }

      // While current message is past next emit time, emit at fixed intervals
      while (next_emit_ < msg->time) {
        output_queue.push_back(create_message<T>(next_emit_, last_value_));
        next_emit_ += dt_;
      }

      // If message exactly on grid point, use its value
      if (next_emit_ == msg->time) {
        output_queue.push_back(create_message<T>(msg->time, msg->data));
        next_emit_ += dt_;
      }

      last_value_ = msg->data;
      input_queue.pop_front();
    }
  }

 private:
  timestamp_t dt_;                 // Fixed time interval
  std::optional<timestamp_t> t0_;  // Optional initial time offset
  timestamp_t next_emit_;          // Next emission time
  bool initialized_;               // Whether first message received
  T last_value_;                   // Last value for causal consistency
};

}  // namespace rtbot

#endif  // RESAMPLER_CONSTANT_H