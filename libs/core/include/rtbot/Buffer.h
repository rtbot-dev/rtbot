/**
 * @brief Numerically Stable Statistical Buffer Implementation
 *
 * Implements online computation of statistics (sum, mean, variance) with numerical stability
 * for streaming data using a sliding window approach.
 *
 * Key algorithms:
 * 1. West, D.H.D (1979). Updating Mean and Variance Estimates: An Improved Method.
 *    For stable variance calculation:
 *    - M2 accumulates squared distances from current mean
 *    - Adding value x:
 *      delta = x - oldMean
 *      newMean = oldMean + delta/n
 *      M2 += delta * (x - newMean)
 *    - Sample variance = M2/(n-1)
 *
 * 2. Kahan, W. (1965). Compensated Summation
 *    Reduces floating-point rounding errors:
 *    - y = value - compensation
 *    - t = sum + y
 *    - compensation = (t - sum) - y
 *    - sum = t
 *
 * State variables:
 * - sum_, sum_compensation_: Running sum with Kahan compensation
 * - M2_, M2_compensation_: Second moment with Kahan compensation
 *
 * Performance:
 * - O(1) statistical updates
 * - O(n) serialization/restore where n = buffer size
 * - Feature templating ensures zero overhead for unused stats
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <cmath>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

struct BufferFeatures {
  static constexpr bool TRACK_SUM = true;
  static constexpr bool TRACK_MEAN = true;
  static constexpr bool TRACK_VARIANCE = true;
};

template <typename T, typename Features = BufferFeatures>
class Buffer : public Operator {
 public:
  Buffer(std::string id, size_t window_size) : Operator(std::move(id)), window_size_(window_size) {
    if (window_size < 1) {
      throw std::runtime_error("Buffer window size must be at least 1");
    }

    add_data_port<T>();
    add_output_port<T>();

    if constexpr (Features::TRACK_SUM || Features::TRACK_MEAN) {
      sum_ = 0.0;
      sum_compensation_ = 0.0;
    }

    if constexpr (Features::TRACK_VARIANCE) {
      M2_ = 0.0;
      M2_compensation_ = 0.0;
    }
  }

  void reset() override {
    Operator::reset();
    buffer_.clear();  // Clear buffer contents
    sum_ = 0.0;       // Reset statistical accumulators
    M2_ = 0.0;
    sum_compensation_ = 0.0;
    M2_compensation_ = 0.0;
  }

  const std::deque<std::unique_ptr<Message<T>>>& buffer() const { return buffer_; }
  size_t buffer_size() const { return buffer_.size(); }
  bool buffer_full() const { return buffer_.size() == window_size_; }
  uint32_t window_size() const { return window_size_; }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_SUM || F::TRACK_MEAN, double> sum() const {
    return sum_;
  }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_MEAN, double> mean() const {
    return buffer_.empty() ? 0.0 : sum_ / buffer_.size();
  }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_VARIANCE, double> variance() const {
    if (buffer_.size() <= 1) return 0.0;
    return M2_ / (buffer_.size() - 1);  // Using n-1 for sample variance
  }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_VARIANCE, double> standard_deviation() const {
    return std::sqrt(variance());
  }

  Bytes collect() override {
    Bytes bytes = Operator::collect();

    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&window_size_),
                 reinterpret_cast<const uint8_t*>(&window_size_) + sizeof(window_size_));

    size_t buffer_size = buffer_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&buffer_size),
                 reinterpret_cast<const uint8_t*>(&buffer_size) + sizeof(buffer_size));

    for (const auto& msg : buffer_) {
      Bytes msg_bytes = msg->serialize();
      size_t msg_size = msg_bytes.size();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&msg_size),
                   reinterpret_cast<const uint8_t*>(&msg_size) + sizeof(msg_size));
      bytes.insert(bytes.end(), msg_bytes.begin(), msg_bytes.end());
    }

    if constexpr (Features::TRACK_SUM || Features::TRACK_MEAN) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sum_),
                   reinterpret_cast<const uint8_t*>(&sum_) + sizeof(sum_));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sum_compensation_),
                   reinterpret_cast<const uint8_t*>(&sum_compensation_) + sizeof(sum_compensation_));
    }

    if constexpr (Features::TRACK_VARIANCE) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&M2_),
                   reinterpret_cast<const uint8_t*>(&M2_) + sizeof(M2_));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&M2_compensation_),
                   reinterpret_cast<const uint8_t*>(&M2_compensation_) + sizeof(M2_compensation_));
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    window_size_ = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    size_t buffer_size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    buffer_.clear();
    for (size_t i = 0; i < buffer_size; ++i) {
      size_t msg_size = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);

      Bytes msg_bytes(it, it + msg_size);
      buffer_.push_back(
          std::unique_ptr<Message<T>>(dynamic_cast<Message<T>*>(BaseMessage::deserialize(msg_bytes).release())));
      it += msg_size;
    }

    if constexpr (Features::TRACK_SUM || Features::TRACK_MEAN) {
      sum_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      sum_compensation_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
    }

    if constexpr (Features::TRACK_VARIANCE) {
      M2_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      M2_compensation_ = *reinterpret_cast<const double*>(&(*it));
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

      std::optional<double> removed_value;
      if (buffer_.size() == window_size_) {
        removed_value = buffer_.front()->data.value;
        buffer_.pop_front();
      }

      // Add new message to buffer
      buffer_.push_back(std::unique_ptr<Message<T>>(dynamic_cast<Message<T>*>(input_queue.front()->clone().release())));

      // Update statistics with added and removed values
      update_statistics(msg->data.value, removed_value);

      auto output_msgs = process_message(msg);
      if (!output_msgs.empty()) {
        for (auto& output_msg : output_msgs) {
          get_output_queue(0).push_back(std::move(output_msg));
        }
      }

      input_queue.pop_front();
    }
  }

  virtual std::vector<std::unique_ptr<Message<T>>> process_message(const Message<T>* msg) = 0;

 private:
  void kahan_add(double& sum, double& compensation, double value) {
    double y = value - compensation;
    double t = sum + y;
    compensation = (t - sum) - y;
    sum = t;
  }

  void update_statistics(double added_value, std::optional<double> removed_value) {
    if constexpr (Features::TRACK_SUM || Features::TRACK_MEAN) {
      if (removed_value) {
        // First remove old value if it exists
        double old_mean = sum_ / buffer_.size();
        kahan_add(sum_, sum_compensation_, -(*removed_value));

        if constexpr (Features::TRACK_VARIANCE) {
          double delta = *removed_value - old_mean;
          kahan_add(M2_, M2_compensation_, -delta * (*removed_value - old_mean));
        }
      }

      // Then add new value
      double old_mean = buffer_.size() > 1 ? (sum_ / (buffer_.size() - 1)) : 0;
      kahan_add(sum_, sum_compensation_, added_value);
      double new_mean = sum_ / buffer_.size();

      if constexpr (Features::TRACK_VARIANCE) {
        double delta = added_value - old_mean;
        kahan_add(M2_, M2_compensation_, delta * (added_value - new_mean));
      }
    }
  }

  size_t window_size_;
  std::deque<std::unique_ptr<Message<T>>> buffer_;
  double sum_{0.0};
  double sum_compensation_{0.0};
  double M2_{0.0};
  double M2_compensation_{0.0};
};

}  // namespace rtbot

#endif  // BUFFER_H