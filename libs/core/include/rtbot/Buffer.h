#ifndef BUFFER_H
#define BUFFER_H

#include <cmath>
#include <deque>
#include <memory>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

// Feature flags for compile-time buffer capabilities
struct BufferFeatures {
  static constexpr bool TRACK_SUM = true;       // Track running sum
  static constexpr bool TRACK_MEAN = true;      // Track running mean
  static constexpr bool TRACK_VARIANCE = true;  // Track running variance (enables std dev)
};

template <typename T, typename Features = BufferFeatures>
class Buffer : public Operator {
 public:
  Buffer(std::string id, size_t window_size) : Operator(std::move(id)), window_size_(window_size) {
    if (window_size < 1) {
      throw std::runtime_error("Buffer window size must be at least 1");
    }

    // Add single input and output port
    add_data_port<T>();
    add_output_port<T>();

    // Initialize statistical trackers if enabled
    if constexpr (Features::TRACK_SUM) {
      sum_ = 0.0;
    }
    if constexpr (Features::TRACK_MEAN) {
      mean_ = 0.0;
      count_ = 0;
    }
    if constexpr (Features::TRACK_VARIANCE) {
      m2_ = 0.0;  // Second moment used for online variance calculation
    }
  }

  // Access buffered data
  const std::deque<T>& buffer() const { return buffer_; }
  size_t buffer_size() const { return buffer_.size(); }
  bool buffer_full() const { return buffer_.size() == window_size_; }

  // Statistical accessors (only available if corresponding features enabled)
  template <typename F = Features>
  std::enable_if_t<F::TRACK_SUM, double> sum() const {
    return sum_;
  }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_MEAN, double> mean() const {
    return count_ > 0 ? mean_ : 0.0;
  }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_VARIANCE, double> variance() const {
    return count_ > 1 ? m2_ / (count_ - 1) : 0.0;
  }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_VARIANCE, double> standard_deviation() const {
    return std::sqrt(variance());
  }

  // State serialization
  Bytes collect() override {
    Bytes bytes = Operator::collect();

    // Serialize window size
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&window_size_),
                 reinterpret_cast<const uint8_t*>(&window_size_) + sizeof(window_size_));

    // Serialize buffer
    size_t buffer_size = buffer_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&buffer_size),
                 reinterpret_cast<const uint8_t*>(&buffer_size) + sizeof(buffer_size));

    for (const auto& value : buffer_) {
      Bytes value_bytes = value.serialize();
      bytes.insert(bytes.end(), value_bytes.begin(), value_bytes.end());
    }

    // Serialize statistical trackers if enabled
    if constexpr (Features::TRACK_SUM) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sum_),
                   reinterpret_cast<const uint8_t*>(&sum_) + sizeof(sum_));
    }
    if constexpr (Features::TRACK_MEAN) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&mean_),
                   reinterpret_cast<const uint8_t*>(&mean_) + sizeof(mean_));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&count_),
                   reinterpret_cast<const uint8_t*>(&count_) + sizeof(count_));
    }
    if constexpr (Features::TRACK_VARIANCE) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&m2_),
                   reinterpret_cast<const uint8_t*>(&m2_) + sizeof(m2_));
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    // Restore window size
    window_size_ = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    // Restore buffer
    size_t buffer_size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    buffer_.clear();
    for (size_t i = 0; i < buffer_size; ++i) {
      T value = T::deserialize(it);
      buffer_.push_back(value);
    }

    // Restore statistical trackers if enabled
    if constexpr (Features::TRACK_SUM) {
      sum_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
    }
    if constexpr (Features::TRACK_MEAN) {
      mean_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      count_ = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);
    }
    if constexpr (Features::TRACK_VARIANCE) {
      m2_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
    }
  }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<T>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Buffer");
      }

      // Update buffer
      buffer_.push_back(msg->data);

      // Update statistical trackers
      update_statistics(msg->data);

      // Remove oldest value if buffer is full
      if (buffer_.size() > window_size_) {
        remove_oldest();
      }

      // Forward message to output if process_message returns true
      if (process_message(msg)) {
        get_output_queue(0).push_back(input_queue.front()->clone());
      }

      input_queue.pop_front();
    }
  }

  // Virtual method to be implemented by derived classes
  virtual bool process_message(const Message<T>* msg) = 0;

 private:
  void update_statistics(const T& value) {
    // Online/one-pass algorithm for statistical calculations
    if constexpr (Features::TRACK_SUM) {
      sum_ += value.value;
    }

    if constexpr (Features::TRACK_MEAN || Features::TRACK_VARIANCE) {
      count_++;
      double delta = value.value - mean_;
      mean_ += delta / count_;

      if constexpr (Features::TRACK_VARIANCE) {
        double delta2 = value.value - mean_;
        m2_ += delta * delta2;
      }
    }
  }

  void remove_oldest() {
    const T& oldest = buffer_.front();

    if constexpr (Features::TRACK_SUM) {
      sum_ -= oldest.value;
    }

    if constexpr (Features::TRACK_MEAN || Features::TRACK_VARIANCE) {
      count_--;
      double delta = oldest.value - mean_;
      mean_ -= delta / count_;

      if constexpr (Features::TRACK_VARIANCE) {
        double delta2 = oldest.value - mean_;
        m2_ -= delta * delta2;
      }
    }

    buffer_.pop_front();
  }

  size_t window_size_;
  std::deque<T> buffer_;

  // Statistical tracking members (only used if corresponding features enabled)
  double sum_{0.0};   // Running sum
  double mean_{0.0};  // Running mean
  size_t count_{0};   // Number of values seen
  double m2_{0.0};    // Second moment for variance calculation
};

}  // namespace rtbot

#endif  // BUFFER_H