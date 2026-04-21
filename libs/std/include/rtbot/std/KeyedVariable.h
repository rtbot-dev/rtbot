#ifndef KEYED_VARIABLE_H
#define KEYED_VARIABLE_H

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "rtbot/Operator.h"
#include "rtbot/StateSerializer.h"

namespace rtbot {

// KeyedVariable — HashMap variant of Variable for reference data lookups.
//
// Ports:
//   i1 (data port 0):    VectorNumberData [key, value]
//                        Updates map[key] = value. NaN value deletes the key.
//   c1 (control port 0): NumberData — key to look up.
//                        Emits BooleanData (exists mode) or NumberData (lookup mode).
//   c2 (control port 1): NumberData — heartbeat.
//                        Advances the timeline to this timestamp without changing state.
//   o1 (output port 0):  BooleanData (exists) or NumberData (lookup).
//
// Processing order when all three arrive at the same timestamp:
//   1. c1 is buffered (left in control queue 0)
//   2. c2 advances heartbeat_time_      (process_control)
//   3. i1 updates the HashMap            (process_data)
//   4. Resolve c1 against updated state (process_data, after i1)
class KeyedVariable : public Operator {
 public:
  KeyedVariable(std::string id, std::string mode = "exists", double default_value = 0.0)
      : Operator(std::move(id)),
        mode_(std::move(mode)),
        default_value_(default_value),
        heartbeat_time_(std::numeric_limits<timestamp_t>::min()),
        data_time_(std::numeric_limits<timestamp_t>::min()) {
    if (mode_ != "exists" && mode_ != "lookup") {
      throw std::runtime_error("KeyedVariable mode must be 'exists' or 'lookup'");
    }
    add_data_port<VectorNumberData>();  // i1: [key, value] changelog
    add_control_port<NumberData>();     // c1: key to look up
    add_control_port<NumberData>();     // c2: heartbeat

    if (mode_ == "exists") {
      add_output_port<BooleanData>();
    } else {
      add_output_port<NumberData>();
    }
  }

  std::string type_name() const override { return "KeyedVariable"; }

  const std::string& get_mode() const { return mode_; }
  double get_default_value() const { return default_value_; }
  size_t hashmap_size() const { return hashmap_.size(); }

  void reset() override {
    Operator::reset();
    hashmap_.clear();
    heartbeat_time_ = std::numeric_limits<timestamp_t>::min();
    data_time_ = std::numeric_limits<timestamp_t>::min();
  }

  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();

    // Serialize HashMap
    size_t map_size = hashmap_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&map_size),
                 reinterpret_cast<const uint8_t*>(&map_size) + sizeof(map_size));
    for (const auto& [k, v] : hashmap_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&k),
                   reinterpret_cast<const uint8_t*>(&k) + sizeof(k));
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&v),
                   reinterpret_cast<const uint8_t*>(&v) + sizeof(v));
    }

    // Serialize timeline state
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&heartbeat_time_),
                 reinterpret_cast<const uint8_t*>(&heartbeat_time_) + sizeof(heartbeat_time_));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&data_time_),
                 reinterpret_cast<const uint8_t*>(&data_time_) + sizeof(data_time_));

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    // Restore HashMap
    size_t map_size;
    std::memcpy(&map_size, &(*it), sizeof(map_size));
    it += sizeof(map_size);

    hashmap_.clear();
    for (size_t i = 0; i < map_size; ++i) {
      double k, v;
      std::memcpy(&k, &(*it), sizeof(k));
      it += sizeof(k);
      std::memcpy(&v, &(*it), sizeof(v));
      it += sizeof(v);
      hashmap_[k] = v;
    }

    // Restore timeline state
    std::memcpy(&heartbeat_time_, &(*it), sizeof(heartbeat_time_));
    it += sizeof(heartbeat_time_);
    std::memcpy(&data_time_, &(*it), sizeof(data_time_));
    it += sizeof(data_time_);
  }

  bool equals(const KeyedVariable& other) const {
    if (mode_ != other.mode_) return false;
    if (StateSerializer::hash_double(default_value_) != StateSerializer::hash_double(other.default_value_))
      return false;
    if (hashmap_.size() != other.hashmap_.size()) return false;
    for (const auto& [k, v] : hashmap_) {
      auto it = other.hashmap_.find(k);
      if (it == other.hashmap_.end()) return false;
      if (StateSerializer::hash_double(v) != StateSerializer::hash_double(it->second)) return false;
    }
    if (heartbeat_time_ != other.heartbeat_time_) return false;
    if (data_time_ != other.data_time_) return false;
    return Operator::equals(other);
  }

  bool operator==(const KeyedVariable& other) const { return equals(other); }
  bool operator!=(const KeyedVariable& other) const { return !(*this == other); }

 protected:
  // process_control: handle c2 (heartbeat) only.
  // c1 (query) is intentionally left in its queue to be resolved in process_data,
  // AFTER i1 has had a chance to update the HashMap for the same timestamp.
  void process_control(bool debug = false) override {
    auto& c2_queue = get_control_queue(1);

    while (!c2_queue.empty()) {
      const auto* msg = static_cast<const Message<NumberData>*>(c2_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid heartbeat message type in KeyedVariable");
      }
      if (msg->time > heartbeat_time_) {
        heartbeat_time_ = msg->time;
      }
      c2_queue.pop_front();
    }
    // c1 (control_queue 0) is left untouched here
  }

  // process_data: update HashMap from i1, then resolve pending c1 queries.
  // A c1 query at time T is resolved when max(heartbeat_time_, data_time_) >= T,
  // ensuring the HashMap reflects all updates at or before T.
  void process_data(bool debug = false) override {
    auto& i1_queue = get_data_queue(0);
    auto& c1_queue = get_control_queue(0);
    // Step 1: apply all pending i1 updates
    while (!i1_queue.empty()) {
      const auto* msg = static_cast<const Message<VectorNumberData>*>(i1_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid data message type in KeyedVariable");
      }
      if (msg->data.values->size() < 2) {
        throw std::runtime_error("KeyedVariable i1 message must have 2 values: [key, value]");
      }

      double key = (*msg->data.values)[0];
      double val = (*msg->data.values)[1];

      if (std::isnan(val)) {
        hashmap_.erase(key);
      } else {
        hashmap_[key] = val;
      }

      data_time_ = msg->time;
      i1_queue.pop_front();
    }

    // Step 2: resolve c1 queries whose timestamp is <= max(heartbeat_time_, data_time_)
    timestamp_t resolve_up_to = std::max(heartbeat_time_, data_time_);

    while (!c1_queue.empty()) {
      const auto* query = static_cast<const Message<NumberData>*>(c1_queue.front().get());
      if (!query) {
        throw std::runtime_error("Invalid query message type in KeyedVariable");
      }
      if (query->time > resolve_up_to) break;

      double lookup_key = query->data.value;
      timestamp_t query_time = query->time;

      if (mode_ == "exists") {
        bool found = hashmap_.count(lookup_key) > 0;
        emit_output(0, create_message<BooleanData>(query_time, BooleanData{found}), debug);
      } else {
        auto it = hashmap_.find(lookup_key);
        double result = (it != hashmap_.end()) ? it->second : default_value_;
        emit_output(0, create_message<NumberData>(query_time, NumberData{result}), debug);
      }

      c1_queue.pop_front();
    }
  }

 private:
  std::string mode_;
  double default_value_;
  std::unordered_map<double, double> hashmap_;
  timestamp_t heartbeat_time_;
  timestamp_t data_time_;
};

inline std::shared_ptr<KeyedVariable> make_keyed_variable(std::string id, std::string mode = "exists",
                                                           double default_value = 0.0) {
  return std::make_shared<KeyedVariable>(std::move(id), std::move(mode), default_value);
}

}  // namespace rtbot

#endif  // KEYED_VARIABLE_H
