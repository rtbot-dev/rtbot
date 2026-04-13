#ifndef WINDOW_MIN_MAX_H
#define WINDOW_MIN_MAX_H

#include <cstring>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

// WindowMinMax: windowed min or max over the last N values using a monotonic
// deque algorithm. O(1) amortized per message. No output until window is full.
class WindowMinMax : public Operator {
 public:
  WindowMinMax(std::string id, size_t window_size, bool is_min)
      : Operator(std::move(id)),
        window_size_(window_size),
        is_min_(is_min),
        pos_(0) {
    if (window_size_ == 0)
      throw std::runtime_error("WindowMinMax: window_size must be positive");
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "WindowMinMax"; }
  size_t window_size() const { return window_size_; }
  bool is_min() const { return is_min_; }

  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&pos_),
                 reinterpret_cast<const uint8_t*>(&pos_) + sizeof(pos_));
    size_t n = mono_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&n),
                 reinterpret_cast<const uint8_t*>(&n) + sizeof(n));
    for (const auto& [p, v] : mono_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&p),
                   reinterpret_cast<const uint8_t*>(&p) + sizeof(p));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&v),
                   reinterpret_cast<const uint8_t*>(&v) + sizeof(v));
    }
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);
    std::memcpy(&pos_, &(*it), sizeof(pos_));
    it += sizeof(pos_);
    size_t n;
    std::memcpy(&n, &(*it), sizeof(n));
    it += sizeof(n);
    mono_.clear();
    for (size_t i = 0; i < n; ++i) {
      size_t p;
      double v;
      std::memcpy(&p, &(*it), sizeof(p));
      it += sizeof(p);
      std::memcpy(&v, &(*it), sizeof(v));
      it += sizeof(v);
      mono_.push_back({p, v});
    }
  }

 protected:
  void process_data(bool /*debug*/ = false) override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg =
          static_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) throw std::runtime_error("WindowMinMax: invalid message type");

      double value = msg->data.value;

      // Update monotonic deque: remove back elements that are dominated
      if (is_min_) {
        while (!mono_.empty() && mono_.back().second >= value) {
          mono_.pop_back();
        }
      } else {
        while (!mono_.empty() && mono_.back().second <= value) {
          mono_.pop_back();
        }
      }
      mono_.push_back({pos_, value});

      // Evict front if it falls outside the window [pos_ - window_size_ + 1, pos_]
      while (mono_.front().first + window_size_ <= pos_) {
        mono_.pop_front();
      }

      // Only emit once the window is full (pos_ >= window_size_ - 1)
      if (pos_ >= window_size_ - 1) {
        output_queue.push_back(create_message<NumberData>(
            msg->time, NumberData{mono_.front().second}));
      }

      ++pos_;
      input_queue.pop_front();
    }
  }

 private:
  size_t window_size_;
  bool is_min_;
  size_t pos_;  // absolute position of next incoming value
  std::deque<std::pair<size_t, double>> mono_;  // (absolute_pos, value)
};

inline std::shared_ptr<WindowMinMax> make_window_min_max(
    std::string id, size_t window_size, const std::string& mode) {
  bool is_min = (mode == "min");
  return std::make_shared<WindowMinMax>(std::move(id), window_size, is_min);
}

}  // namespace rtbot

#endif  // WINDOW_MIN_MAX_H
