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
    last_value_ = T{};
  }

  void reset() override {
    Operator::reset();
    initialized_ = false;
    next_emit_ = 0;
    last_value_ = T{};  // Reset to default constructed value
  }

  std::string type_name() const override { return "ResamplerConstant"; }

  bool equals(const ResamplerConstant& other) const {
      
      if (dt_ != other.dt_) return false;
      if (t0_ != other.t0_) return false;
      if (initialized_ != other.initialized_) return false;
      if (next_emit_ != other.next_emit_) return false;
      if (last_value_ != other.last_value_) return false;

      if (!Operator::equals(other)) return false;

      return true;
  }
  
  bool operator==(const ResamplerConstant& other) const {
    return equals(other);
  }

  bool operator!=(const ResamplerConstant& other) const {
    return !(*this == other);
  }

  Bytes collect() override {
    // First collect base state
    Bytes bytes = Operator::collect();

    // Serialize next emission time
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&next_emit_),
                 reinterpret_cast<const uint8_t*>(&next_emit_) + sizeof(next_emit_));

    // Serialize initialization state
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&initialized_),
                 reinterpret_cast<const uint8_t*>(&initialized_) + sizeof(initialized_));

    // Serialize last value
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&last_value_),
                reinterpret_cast<const uint8_t*>(&last_value_) + sizeof(last_value_));

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    // ---- Restore base state ----
    Operator::restore(it);

    // ---- Restore next_emit_ safely ----
    std::memcpy(&next_emit_, &(*it), sizeof(next_emit_));
    it += sizeof(next_emit_);

    // ---- Restore initialized_ safely ----
    std::memcpy(&initialized_, &(*it), sizeof(initialized_));
    it += sizeof(initialized_);

    // ---- Restore last_value_ safely ----
    std::memcpy(&last_value_, &(*it), sizeof(last_value_));
    it += sizeof(last_value_);
  }

  timestamp_t get_interval() const { return dt_; }
  timestamp_t get_next_emission_time() const { return next_emit_; }
  std::optional<timestamp_t> get_t0() const { return t0_; }

 protected:
  void process_data(bool debug=false) override {
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

inline std::shared_ptr<Operator> make_resampler_constant(const std::string& id, timestamp_t interval,
                                                         std::optional<timestamp_t> t0 = std::nullopt) {
  return std::make_shared<ResamplerConstant<NumberData>>(id, interval, t0);
}

}  // namespace rtbot

#endif  // RESAMPLER_CONSTANT_H