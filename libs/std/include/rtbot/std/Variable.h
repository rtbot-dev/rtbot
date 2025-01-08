#ifndef VARIABLE_H
#define VARIABLE_H

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Operator.h"
#include "rtbot/StateSerializer.h"

namespace rtbot {

class Variable : public Operator {
 private:
  struct TimeValue {
    timestamp_t time;
    double value;
  };

 public:
  Variable(std::string id, double default_value = 0.0) : Operator(std::move(id)), default_value_(default_value) {
    add_data_port<NumberData>();     // For value updates
    add_control_port<NumberData>();  // For queries
    add_output_port<NumberData>();   // For responses

    values_.push_back({0, default_value_});
  }

  void reset() override {
    Operator::reset();
    values_.clear();
    values_.push_back({0, default_value_});
    pending_queries_.clear();
  }

  std::string type_name() const override { return "Variable"; }

  double get_default_value() const { return default_value_; }

  Bytes collect() override {
    Bytes bytes = Operator::collect();

    // Serialize default value
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&default_value_),
                 reinterpret_cast<const uint8_t*>(&default_value_) + sizeof(default_value_));

    // Serialize time-value pairs
    size_t values_count = values_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&values_count),
                 reinterpret_cast<const uint8_t*>(&values_count) + sizeof(values_count));

    for (const auto& tv : values_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&tv.time),
                   reinterpret_cast<const uint8_t*>(&tv.time) + sizeof(tv.time));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&tv.value),
                   reinterpret_cast<const uint8_t*>(&tv.value) + sizeof(tv.value));
    }

    // Serialize pending query timestamps
    size_t queries_count = pending_queries_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&queries_count),
                 reinterpret_cast<const uint8_t*>(&queries_count) + sizeof(queries_count));

    for (const auto& time : pending_queries_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                   reinterpret_cast<const uint8_t*>(&time) + sizeof(time));
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    // Restore default value
    default_value_ = *reinterpret_cast<const double*>(&(*it));
    it += sizeof(double);

    // Restore time-value pairs
    size_t values_count = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    values_.clear();
    values_.reserve(values_count);
    for (size_t i = 0; i < values_count; ++i) {
      timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
      it += sizeof(timestamp_t);
      double value = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      values_.push_back({time, value});
    }

    // Restore pending query timestamps
    size_t queries_count = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    pending_queries_.clear();
    pending_queries_.reserve(queries_count);
    for (size_t i = 0; i < queries_count; ++i) {
      timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
      it += sizeof(timestamp_t);
      pending_queries_.push_back(time);
    }
  }

 protected:
  void process_data() override {
    auto& data_queue = get_data_queue(0);
    bool values_updated = false;

    while (!data_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(data_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Variable");
      }

      values_.push_back({msg->time, msg->data.value});
      values_updated = true;
      data_queue.pop_front();
    }

    if (values_updated) {
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

      pending_queries_.push_back(query->time);
      control_queue.pop_front();
    }

    process_pending_queries();
  }

 private:
  double default_value_;
  std::vector<TimeValue> values_;
  std::vector<timestamp_t> pending_queries_;

  std::optional<double> query_value(timestamp_t time) {
    if (values_.size() == 1 && time == values_[0].time) {
      return values_[0].value;
    }

    if (values_.size() > 1) {
      // Find the last value that occurred at or before the query time
      for (size_t i = 1; i < values_.size(); i++) {
        if (time == values_[i].time) {
          // Found exact match - clean up all ranges before this one
          double value = values_[i].value;
          for (size_t j = 0; j < i; j++) {
            values_.erase(values_.begin());
          }
          return value;
        } else if (values_[i - 1].time <= time && time < values_[i].time) {
          // Found containing range - clean up all ranges before the starting range
          double value = values_[i - 1].value;
          for (size_t j = 0; j < i - 1; j++) {
            values_.erase(values_.begin());
          }
          return value;
        }
      }
    }

    return std::nullopt;
  }

  void process_pending_queries() {
    auto& output_queue = get_output_queue(0);
    size_t processed = 0;

    for (size_t i = 0; i < pending_queries_.size(); i++) {
      timestamp_t query_time = pending_queries_[i];
      auto value = query_value(query_time);
      if (!value) {
        break;  // Stop at first uncertain value
      }

      output_queue.push_back(create_message<NumberData>(query_time, NumberData{*value}));
      processed++;
    }

    // Remove processed queries
    if (processed > 0) {
      pending_queries_.erase(pending_queries_.begin(), pending_queries_.begin() + processed);
    }
  }
};

// Factory function
inline std::unique_ptr<Variable> make_variable(std::string id, double default_value = 0.0) {
  return std::make_unique<Variable>(std::move(id), default_value);
}

}  // namespace rtbot

#endif  // VARIABLE_H