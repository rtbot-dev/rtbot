#ifndef VARIABLE_H
#define VARIABLE_H

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "rtbot/Operator.h"
#include "rtbot/StateSerializer.h"

namespace rtbot {

class Variable : public Operator {
 private:
  struct TimeRange {
    timestamp_t start;  // Inclusive
    timestamp_t end;    // Exclusive
    double value;
  };

 public:
  Variable(std::string id, double default_value = 0.0) : Operator(std::move(id)), default_value_(default_value) {
    add_data_port<NumberData>();     // For value updates
    add_control_port<NumberData>();  // For queries
    add_output_port<NumberData>();   // For responses
  }

  void reset() override {
    Operator::reset();
    ranges_.clear();
    pending_queries_.clear();
  }

  std::string type_name() const override { return "Variable"; }

  double get_default_value() const { return default_value_; }

  Bytes collect() override {
    Bytes bytes = Operator::collect();

    // Serialize default value
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&default_value_),
                 reinterpret_cast<const uint8_t*>(&default_value_) + sizeof(default_value_));

    // Serialize ranges
    size_t ranges_count = ranges_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&ranges_count),
                 reinterpret_cast<const uint8_t*>(&ranges_count) + sizeof(ranges_count));

    for (const auto& range : ranges_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&range.start),
                   reinterpret_cast<const uint8_t*>(&range.start) + sizeof(range.start));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&range.end),
                   reinterpret_cast<const uint8_t*>(&range.end) + sizeof(range.end));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&range.value),
                   reinterpret_cast<const uint8_t*>(&range.value) + sizeof(range.value));
    }

    // Serialize pending queries
    size_t queries_count = pending_queries_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&queries_count),
                 reinterpret_cast<const uint8_t*>(&queries_count) + sizeof(queries_count));

    for (const auto& query : pending_queries_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&query.time),
                   reinterpret_cast<const uint8_t*>(&query.time) + sizeof(query.time));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&query.data.value),
                   reinterpret_cast<const uint8_t*>(&query.data.value) + sizeof(query.data.value));
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    // Restore default value
    default_value_ = *reinterpret_cast<const double*>(&(*it));
    it += sizeof(double);

    // Restore ranges
    size_t ranges_count = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    ranges_.clear();
    for (size_t i = 0; i < ranges_count; ++i) {
      TimeRange range;
      range.start = *reinterpret_cast<const timestamp_t*>(&(*it));
      it += sizeof(timestamp_t);
      range.end = *reinterpret_cast<const timestamp_t*>(&(*it));
      it += sizeof(timestamp_t);
      range.value = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      ranges_.push_back(range);
    }

    // Restore pending queries
    size_t queries_count = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    pending_queries_.clear();
    for (size_t i = 0; i < queries_count; ++i) {
      timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
      it += sizeof(timestamp_t);
      double value = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      pending_queries_.push_back(Message<NumberData>(time, NumberData{value}));
    }
  }

 protected:
  void process_data() override {
    auto& data_queue = get_data_queue(0);
    bool ranges_updated = false;

    while (!data_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(data_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Variable");
      }

      update_ranges(msg->time, msg->data.value);
      ranges_updated = true;
      data_queue.pop_front();
    }

    if (ranges_updated) {
      process_pending_queries();
    }
  }

  void process_control() override {
    auto& control_queue = get_control_queue(0);

    while (!control_queue.empty()) {
      const auto* query = dynamic_cast<const Message<NumberData>*>(control_queue.front().get());
      if (!query) {
        throw std::runtime_error("Invalid control message type in Variable");
      }

      pending_queries_.push_back(*query);
      control_queue.pop_front();
    }

    process_pending_queries();
  }

 private:
  double default_value_;
  std::vector<TimeRange> ranges_;
  std::vector<Message<NumberData>> pending_queries_;

  void update_ranges(timestamp_t time, double value) {
    if (ranges_.empty()) {
      ranges_.push_back({0, time, default_value_});
    }

    ranges_.back().end = time;
    ranges_.push_back({time, 0, value});  // End time doesn't matter as it will be set by next update
  }

  std::optional<double> query_value(timestamp_t time) const {
    if (ranges_.empty() || time < ranges_[0].start) {
      return std::nullopt;
    }

    // From the last range we are only sure about the value at start time
    if (time == ranges_.back().start) {
      return ranges_.back().value;
    }

    for (size_t i = 0; i < ranges_.size() - 1; i++) {
      if (time >= ranges_[i].start && time < ranges_[i].end) {
        return ranges_[i].value;
      }
    }

    return std::nullopt;
  }

  void process_pending_queries() {
    auto& output_queue = get_output_queue(0);
    size_t processed = 0;

    for (const auto& query : pending_queries_) {
      auto value = query_value(query.time);
      if (!value) {
        break;  // Stop at first uncertain value
      }

      output_queue.push_back(create_message<NumberData>(query.time, NumberData{*value}));

      // Remove all ranges that end before this query time
      while (!ranges_.empty() && ranges_[0].end <= query.time) {
        ranges_.erase(ranges_.begin());
      }
      processed++;
    }

    // Remove processed queries
    pending_queries_.erase(pending_queries_.begin(), pending_queries_.begin() + processed);
  }
};

// Factory function
inline std::unique_ptr<Variable> make_variable(std::string id, double default_value = 0.0) {
  return std::make_unique<Variable>(std::move(id), default_value);
}

}  // namespace rtbot

#endif  // VARIABLE_H