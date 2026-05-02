#ifndef MIN_MAX_TRACKER_H
#define MIN_MAX_TRACKER_H

#include <cstring>
#include <limits>
#include <memory>
#include <string>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

// MinTracker: all-time running minimum. Emits current min on every message.
class MinTracker : public Operator {
 public:
  explicit MinTracker(std::string id)
      : Operator(std::move(id)),
        min_(std::numeric_limits<double>::infinity()) {
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "MinTracker"; }
  double get_current_min() const { return min_; }

  void reset() override {
    Operator::reset();
    min_ = std::numeric_limits<double>::infinity();
  }

  bool equals(const MinTracker& other) const {
    return StateSerializer::hash_double(min_) ==
               StateSerializer::hash_double(other.min_) &&
           Operator::equals(other);
  }
  bool operator==(const MinTracker& other) const { return equals(other); }
  bool operator!=(const MinTracker& other) const { return !(*this == other); }

  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&min_),
                 reinterpret_cast<const uint8_t*>(&min_) + sizeof(min_));
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);
    std::memcpy(&min_, &(*it), sizeof(min_));
    it += sizeof(min_);
  }

 protected:
  void process_data(bool debug = false) override {
    auto& input_queue = get_data_queue(0);
    if (input_queue.empty()) return;
    if (input_queue.size() >= kEmitBatchThreshold) {
      std::vector<std::unique_ptr<BaseMessage>> batch;
      batch.reserve(input_queue.size());
      while (!input_queue.empty()) {
        const auto* msg =
            static_cast<const Message<NumberData>*>(input_queue.front().get());
        if (!msg) {
          throw std::runtime_error("Invalid message type in MinTracker");
        }
        if (msg->data.value < min_) min_ = msg->data.value;
        batch.push_back(create_message<NumberData>(msg->time, NumberData{min_}));
        input_queue.pop_front();
      }
      emit_output(0, std::move(batch), debug);
    } else {
      while (!input_queue.empty()) {
        const auto* msg =
            static_cast<const Message<NumberData>*>(input_queue.front().get());
        if (!msg) {
          throw std::runtime_error("Invalid message type in MinTracker");
        }
        if (msg->data.value < min_) min_ = msg->data.value;
        emit_output(0,
            create_message<NumberData>(msg->time, NumberData{min_}), debug);
        input_queue.pop_front();
      }
    }
  }

 private:
  double min_;
};

// MaxTracker: all-time running maximum. Emits current max on every message.
class MaxTracker : public Operator {
 public:
  explicit MaxTracker(std::string id)
      : Operator(std::move(id)),
        max_(-std::numeric_limits<double>::infinity()) {
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "MaxTracker"; }
  double get_current_max() const { return max_; }

  void reset() override {
    Operator::reset();
    max_ = -std::numeric_limits<double>::infinity();
  }

  bool equals(const MaxTracker& other) const {
    return StateSerializer::hash_double(max_) ==
               StateSerializer::hash_double(other.max_) &&
           Operator::equals(other);
  }
  bool operator==(const MaxTracker& other) const { return equals(other); }
  bool operator!=(const MaxTracker& other) const { return !(*this == other); }

  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&max_),
                 reinterpret_cast<const uint8_t*>(&max_) + sizeof(max_));
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);
    std::memcpy(&max_, &(*it), sizeof(max_));
    it += sizeof(max_);
  }

 protected:
  void process_data(bool debug = false) override {
    auto& input_queue = get_data_queue(0);
    if (input_queue.empty()) return;
    if (input_queue.size() >= kEmitBatchThreshold) {
      std::vector<std::unique_ptr<BaseMessage>> batch;
      batch.reserve(input_queue.size());
      while (!input_queue.empty()) {
        const auto* msg =
            static_cast<const Message<NumberData>*>(input_queue.front().get());
        if (!msg) {
          throw std::runtime_error("Invalid message type in MaxTracker");
        }
        if (msg->data.value > max_) max_ = msg->data.value;
        batch.push_back(create_message<NumberData>(msg->time, NumberData{max_}));
        input_queue.pop_front();
      }
      emit_output(0, std::move(batch), debug);
    } else {
      while (!input_queue.empty()) {
        const auto* msg =
            static_cast<const Message<NumberData>*>(input_queue.front().get());
        if (!msg) {
          throw std::runtime_error("Invalid message type in MaxTracker");
        }
        if (msg->data.value > max_) max_ = msg->data.value;
        emit_output(0,
            create_message<NumberData>(msg->time, NumberData{max_}), debug);
        input_queue.pop_front();
      }
    }
  }

 private:
  double max_;
};

inline std::shared_ptr<MinTracker> make_min_tracker(std::string id) {
  return std::make_shared<MinTracker>(std::move(id));
}

inline std::shared_ptr<MaxTracker> make_max_tracker(std::string id) {
  return std::make_shared<MaxTracker>(std::move(id));
}

}  // namespace rtbot

#endif  // MIN_MAX_TRACKER_H
