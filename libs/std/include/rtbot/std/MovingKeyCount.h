#ifndef MOVING_KEY_COUNT_H
#define MOVING_KEY_COUNT_H

#include <cstring>
#include <deque>
#include <unordered_map>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

// MovingKeyCount: for each incoming key value, counts how many times that
// key appeared in the last window_size messages (sliding window).
//
// State: O(N) ring buffer of recent keys + O(K) HashMap of per-key counts.
// Complexity: O(1) per message.
// Use case: pre-filter for HAVING MOVING_COUNT(N) > threshold before
//           KeyedPipeline, so only active keys pass through.
class MovingKeyCount : public Operator {
 public:
  explicit MovingKeyCount(std::string id, size_t window_size)
      : Operator(std::move(id)), window_size_(window_size) {
    if (window_size < 1) {
      throw std::runtime_error(
          "MovingKeyCount: window_size must be at least 1");
    }
    add_data_port<NumberData>();   // i1 — key value
    add_output_port<NumberData>();  // o1 — count of this key in window
  }

  std::string type_name() const override { return "MovingKeyCount"; }
  size_t get_window_size() const { return window_size_; }

  bool equals(const MovingKeyCount& other) const {
    return window_size_ == other.window_size_ && Operator::equals(other);
  }
  bool operator==(const MovingKeyCount& other) const { return equals(other); }
  bool operator!=(const MovingKeyCount& other) const { return !(*this == other); }

  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();

    // Ring buffer contents
    size_t n = ring_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&n),
                 reinterpret_cast<const uint8_t*>(&n) + sizeof(n));
    for (double k : ring_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&k),
                   reinterpret_cast<const uint8_t*>(&k) + sizeof(k));
    }

    // Per-key counts
    size_t m = counts_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&m),
                 reinterpret_cast<const uint8_t*>(&m) + sizeof(m));
    for (const auto& [k, v] : counts_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&k),
                   reinterpret_cast<const uint8_t*>(&k) + sizeof(k));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&v),
                   reinterpret_cast<const uint8_t*>(&v) + sizeof(v));
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    size_t n;
    std::memcpy(&n, &(*it), sizeof(n));
    it += sizeof(n);
    ring_.clear();
    for (size_t i = 0; i < n; i++) {
      double k;
      std::memcpy(&k, &(*it), sizeof(k));
      it += sizeof(k);
      ring_.push_back(k);
    }

    size_t m;
    std::memcpy(&m, &(*it), sizeof(m));
    it += sizeof(m);
    counts_.clear();
    for (size_t i = 0; i < m; i++) {
      double k;
      size_t v;
      std::memcpy(&k, &(*it), sizeof(k));
      it += sizeof(k);
      std::memcpy(&v, &(*it), sizeof(v));
      it += sizeof(v);
      counts_[k] = v;
    }
  }

 protected:
  void process_data(bool debug = false) override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg =
          dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in MovingKeyCount");
      }

      double key = msg->data.value;

      // Evict oldest key from window if at capacity
      if (ring_.size() >= window_size_) {
        double old_key = ring_.front();
        ring_.pop_front();
        auto it = counts_.find(old_key);
        if (it != counts_.end()) {
          if (it->second <= 1) {
            counts_.erase(it);
          } else {
            it->second--;
          }
        }
      }

      // Insert new key
      ring_.push_back(key);
      counts_[key]++;

      // Emit count of this key in the current window
      output_queue.push_back(create_message<NumberData>(
          msg->time, NumberData{static_cast<double>(counts_[key])}));
      input_queue.pop_front();
    }
  }

 private:
  size_t window_size_;
  std::deque<double> ring_;
  std::unordered_map<double, size_t> counts_;
};

inline std::shared_ptr<MovingKeyCount> make_moving_key_count(std::string id,
                                                              size_t window_size) {
  return std::make_shared<MovingKeyCount>(std::move(id), window_size);
}

}  // namespace rtbot

#endif  // MOVING_KEY_COUNT_H
