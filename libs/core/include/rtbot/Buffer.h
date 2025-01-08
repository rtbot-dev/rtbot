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

    if constexpr (Features::TRACK_SUM) {
      sum_ = 0.0;
    }

    if constexpr (Features::TRACK_VARIANCE) {
      M2_ = 0.0;
    }
  }

  void reset() override {
    Operator::reset();
    buffer_.clear();  // Clear buffer contents
    sum_ = 0.0;       // Reset statistical accumulators
    M2_ = 0.0;
  }

  const std::deque<std::unique_ptr<Message<T>>>& buffer() const { return buffer_; }
  size_t buffer_size() const { return buffer_.size(); }
  bool buffer_full() const { return buffer_.size() == window_size_; }
  uint32_t window_size() const { return window_size_; }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_SUM, double> sum() const {
    return sum_;
  }

  template <typename F = Features>
  std::enable_if_t<F::TRACK_SUM, double> mean() const {
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

    if constexpr (Features::TRACK_SUM) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&sum_),
                   reinterpret_cast<const uint8_t*>(&sum_) + sizeof(sum_));
    }

    if constexpr (Features::TRACK_VARIANCE) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&M2_),
                   reinterpret_cast<const uint8_t*>(&M2_) + sizeof(M2_));
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

    if constexpr (Features::TRACK_SUM) {
      sum_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
    }

    if constexpr (Features::TRACK_VARIANCE) {
      M2_ = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);

      // Recompute statistics from buffer to ensure consistency
      sum_ = 0.0;
      M2_ = 0.0;

      if (!buffer_.empty()) {
        // First pass: compute mean
        for (const auto& msg : buffer_) {
          sum_ += msg->data.value;
        }

        // Second pass: compute M2
        double mean = sum_ / buffer_.size();
        for (const auto& msg : buffer_) {
          double delta = msg->data.value - mean;
          M2_ += delta * delta;
        }
      }
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
  void update_statistics(double added_value, std::optional<double> removed_value) {
    if constexpr (Features::TRACK_SUM && !Features::TRACK_VARIANCE) {
      if (removed_value) {
        sum_ -= *removed_value;
      }
      sum_ += added_value;
      return;
    }

    if constexpr (Features::TRACK_VARIANCE) {
      if (removed_value) {
        sum_ -= *removed_value;
      }
      sum_ += added_value;

      M2_ = 0.0;

      if (buffer_.size() <= 1) return;

      double mean = sum_ / buffer_.size();

      // Second pass: compute M2
      for (const auto& msg : buffer_) {
        double delta = msg->data.value - mean;
        M2_ += delta * delta;
      }
    }
  }

  size_t window_size_;
  std::deque<std::unique_ptr<Message<T>>> buffer_;
  double sum_{0.0};
  double M2_{0.0};
};

}  // namespace rtbot

#endif  // BUFFER_H