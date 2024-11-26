/**
 * @brief Buffer Class Numerical Stability Implementation Details
 *
 * The Buffer class implements several numerical stability optimizations to handle
 * floating-point arithmetic accurately over long sequences of additions and subtractions.
 *
 * Numerical Stability Features:
 *
 * 1. Kahan Summation Algorithm
 *    The class uses the Kahan summation algorithm (also known as compensated summation)
 *    to maintain accurate running sums. This algorithm significantly reduces numerical
 *    error in long sums by tracking a compensation term that accounts for lost low-order bits.
 *
 *    For a sequence: sum += value
 *    Standard addition accumulates errors:
 *      sum = sum + value
 *
 *    Kahan summation corrects this:
 *      y = value - compensation
 *      t = sum + y
 *      compensation = (t - sum) - y
 *      sum = t
 *
 *    The compensation term keeps track of the rounding error and feeds it back into
 *    the next addition.
 *
 * 2. Incremental Updates
 *    Rather than recomputing sums over the entire buffer, we maintain running totals
 *    that are updated incrementally when values are added or removed:
 *    - New values are added using Kahan summation
 *    - Removed values are subtracted using Kahan summation with negated values
 *    This maintains O(1) update complexity while preserving numerical stability.
 *
 * 3. Statistical Computations
 *    - Sum: Directly uses Kahan summation with compensation
 *    - Mean: Calculated from compensated sum divided by count
 *    - Variance: Uses compensated sum of squares and compensated sum to maintain stability
 *
 * 4. Feature Gates
 *    Statistical tracking is controlled by template parameters:
 *    - TRACK_SUM: Enables sum tracking with compensation
 *    - TRACK_MEAN: Enables mean calculation (requires sum)
 *    - TRACK_VARIANCE: Enables variance calculation with separate squared sum tracking
 *
 * Example of how the compensation works:
 * ```
 * // Adding value 1.0e-16 to sum 1.0e0
 * Without compensation:
 *   1.0 + 1.0e-16 = 1.0  (information lost)
 *
 * With Kahan compensation:
 *   y = 1.0e-16 - 0.0
 *   t = 1.0 + 1.0e-16
 *   compensation = (t - 1.0) - 1.0e-16
 *   sum = t
 * ```
 *
 * Memory Layout:
 * The class maintains separate compensation terms for different calculations:
 * - sum_compensation_: For basic sum calculations
 * - squared_sum_compensation_: For sum of squares (variance calculation)
 *
 * Performance Considerations:
 * - All statistical updates remain O(1)
 * - Small memory overhead for compensation terms
 * - Slightly more FLOPs per update, but better numerical accuracy
 * - Feature gates ensure no overhead for unused calculations
 *
 * References:
 * - Kahan, W. (1965). "Further remarks on reducing truncation errors"
 * - Higham, N. J. "The accuracy of floating point summation"
 */
#ifndef BUFFER_H
#define BUFFER_H

#include <cmath>
#include <deque>
#include <memory>
#include <optional>

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

    // Initialize statistical accumulators and compensation terms
    if constexpr (Features::TRACK_SUM || Features::TRACK_MEAN) {
      sum_ = 0.0;
      sum_compensation_ = 0.0;
    }

    if constexpr (Features::TRACK_VARIANCE) {
      squared_sum_ = 0.0;
      squared_sum_compensation_ = 0.0;
    }
  }

  // Access buffered data
  const std::deque<T>& buffer() const { return buffer_; }
  size_t buffer_size() const { return buffer_.size(); }
  bool buffer_full() const { return buffer_.size() == window_size_; }

  // Statistical accessors
  template <typename F = Features>
  std::enable_if_t<F::TRACK_SUM, double> sum() const {
    return sum_;
  }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_MEAN, double> mean() const {
    return buffer_.empty() ? 0.0 : sum_ / buffer_.size();
  }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_VARIANCE, double> variance() const {
    if (buffer_.size() <= 1) return 0.0;
    double m = mean();
    double n = buffer_.size();
    return (squared_sum_ - n * m * m) / (n - 1);
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

    // Serialize statistical accumulators and compensation terms
    if constexpr (Features::TRACK_SUM || Features::TRACK_MEAN) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sum_),
                   reinterpret_cast<const uint8_t*>(&sum_) + sizeof(sum_));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sum_compensation_),
                   reinterpret_cast<const uint8_t*>(&sum_compensation_) + sizeof(sum_compensation_));
    }

    if constexpr (Features::TRACK_VARIANCE) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&squared_sum_),
                   reinterpret_cast<const uint8_t*>(&squared_sum_) + sizeof(squared_sum_));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&squared_sum_compensation_),
                   reinterpret_cast<const uint8_t*>(&squared_sum_compensation_) + sizeof(squared_sum_compensation_));
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

    // Restore statistical accumulators and compensation terms
    if constexpr (Features::TRACK_SUM || Features::TRACK_MEAN) {
      sum_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      sum_compensation_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
    }

    if constexpr (Features::TRACK_VARIANCE) {
      squared_sum_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      squared_sum_compensation_ = *reinterpret_cast<const double*>(&(*it));
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

      // Update statistics with new value using Kahan summation
      add_value(msg->data.value);

      // Remove oldest value if buffer is full
      if (buffer_.size() > window_size_) {
        remove_value(buffer_.front().value);
        buffer_.pop_front();
      }

      // Process message and forward to output if needed
      auto output_msg = process_message(msg);
      if (output_msg) {
        get_output_queue(0).push_back(std::move(output_msg));
      }

      input_queue.pop_front();
    }
  }

  // Virtual method to be implemented by derived classes
  virtual std::unique_ptr<Message<T>> process_message(const Message<T>* msg) = 0;

 private:
  void add_value(double value) {
    if constexpr (Features::TRACK_SUM || Features::TRACK_MEAN) {
      double y = value - sum_compensation_;
      double t = sum_ + y;
      sum_compensation_ = (t - sum_) - y;
      sum_ = t;
    }

    if constexpr (Features::TRACK_VARIANCE) {
      double squared = value * value;
      double y = squared - squared_sum_compensation_;
      double t = squared_sum_ + y;
      squared_sum_compensation_ = (t - squared_sum_) - y;
      squared_sum_ = t;
    }
  }

  void remove_value(double value) {
    if constexpr (Features::TRACK_SUM || Features::TRACK_MEAN) {
      double y = -value - sum_compensation_;
      double t = sum_ + y;
      sum_compensation_ = (t - sum_) - y;
      sum_ = t;
    }

    if constexpr (Features::TRACK_VARIANCE) {
      double squared = value * value;
      double y = -squared - squared_sum_compensation_;
      double t = squared_sum_ + y;
      squared_sum_compensation_ = (t - squared_sum_) - y;
      squared_sum_ = t;
    }
  }

  size_t window_size_;
  std::deque<T> buffer_;

  // Statistical accumulators with compensation terms for numerical stability
  double sum_{0.0};
  double sum_compensation_{0.0};
  double squared_sum_{0.0};
  double squared_sum_compensation_{0.0};
};

}  // namespace rtbot

#endif  // BUFFER_H