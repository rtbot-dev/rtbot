#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

#include <cstddef>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>

#include "Operator.h"
#include "StateSerializer.h"
#include "TimestampTracker.h"

namespace rtbot {

template <typename T>
class Multiplexer : public Operator {
 public:
  Multiplexer(std::string id, size_t num_ports) : Operator(std::move(id)) {
    if (num_ports < 1) {
      throw std::runtime_error("Number of ports must be at least 1");
    }

    // Add data ports
    for (size_t i = 0; i < num_ports; ++i) {
      add_data_port<T>();
      data_time_tracker_[i] = std::set<timestamp_t>();
    }

    // Add corresponding control ports
    for (size_t i = 0; i < num_ports; ++i) {
      add_control_port<BooleanData>();
      control_time_tracker_[i] = std::map<timestamp_t, bool>();
    }

    // Single output port
    add_output_port<T>();
  }

  void reset() override {
    Operator::reset();
    data_time_tracker_.clear();
    control_time_tracker_.clear();
    
    for (size_t i = 0; i < get_num_ports(); ++i) {      
      data_time_tracker_[i] = std::set<timestamp_t>();
    }
   
    for (size_t i = 0; i < get_num_ports(); ++i) {      
      control_time_tracker_[i] = std::map<timestamp_t, bool>();
    }
  }

  size_t get_num_ports() const { return data_ports_.size(); }

  std::string type_name() const override { return "Multiplexer"; }

  // State serialization
  Bytes collect() override {
    // First collect base state
    Bytes bytes = Operator::collect();

    // Serialize data time tracker
    StateSerializer::serialize_port_timestamp_set_map(bytes, data_time_tracker_);

    // Serialize control time tracker
    StateSerializer::serialize_port_control_map(bytes, control_time_tracker_);

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    // First restore base state
    Operator::restore(it);

    // Clear current state
    data_time_tracker_.clear();
    control_time_tracker_.clear();

    // Restore data time tracker
    StateSerializer::deserialize_port_timestamp_set_map(it, data_time_tracker_);
    StateSerializer::validate_port_count(data_time_tracker_.size(), num_data_ports(), "Data");

    // Restore control time tracker
    StateSerializer::deserialize_port_control_map(it, control_time_tracker_);
    StateSerializer::validate_port_count(control_time_tracker_.size(), num_control_ports(), "Control");
  }

  timestamp_t receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    auto time = msg->time;
    timestamp_t time_dequeued = Operator::receive_data(std::move(msg), port_index);

    data_time_tracker_[port_index].insert(time);
    if (time_dequeued >= 0) {      
      data_time_tracker_[port_index].erase(time_dequeued);
    }

    return time_dequeued;
  }

  timestamp_t receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    if (port_index >= num_control_ports()) {
      throw std::runtime_error("Invalid control port index");
    }

    auto* ctrl_msg = dynamic_cast<const Message<BooleanData>*>(msg.get());
    if (!ctrl_msg) {
      throw std::runtime_error("Invalid control message type");
    }

    timestamp_t time_dequeued = -1;

    if (get_control_queue(port_index).size() == max_size_per_port_) {
      time_dequeued = get_control_queue(port_index).front()->time;
      get_control_queue(port_index).pop_front();
    }

    // Update control time tracker
    control_time_tracker_[port_index][ctrl_msg->time] = ctrl_msg->data.value;

    if (time_dequeued >= 0) {
      auto it = control_time_tracker_.find(port_index);
      if (it != control_time_tracker_.end()) {
          it->second.erase(time_dequeued);
      }
    }

    // Add message to queue
    get_control_queue(port_index).push_back(std::move(msg));
    data_ports_with_new_data_.insert(port_index);

    return time_dequeued;
  }

 protected:
  void process_data() override {
    while (true) {
      // Find the next timestamp that exists in all data queues
      auto common_data_time = TimestampTracker::find_oldest_common_time(data_time_tracker_);

      // Clean up old control data if we have a common data time
      if (common_data_time) {
        clean_old_control_timestamps(*common_data_time);
      }

      // Look for matching control messages
      auto common_control_time = TimestampTracker::find_oldest_common_time(
          control_time_tracker_, common_data_time.value_or(std::numeric_limits<timestamp_t>::min()));
      if (!common_control_time) {
        break;
      }

      auto port_to_emit = find_port_to_emit(*common_control_time);
      if (!port_to_emit) {
        clean_up_control_messages(*common_control_time);
        continue;
      }

      auto& input_queue = get_data_queue(*port_to_emit);
      bool message_found = false;

      if (!input_queue.empty()) {
        message_found = false;
        for (const auto& msg_ptr : input_queue) {
          if (auto* msg = dynamic_cast<const Message<T>*>(msg_ptr.get())) {
            if (msg->time == *common_control_time) {
              auto& output = get_output_queue(0);
              output.push_back(create_message<T>(msg->time, msg->data));
              message_found = true;
              break;  // Found our message for this timestamp
            } else if (msg->time > *common_control_time) {
              break;  // No need to check further as messages are ordered by time
            }
          }
        }
      }

      clean_up_control_messages(*common_control_time);

      if (message_found) {
        clean_up_data_messages(*common_control_time);
      } else {
        break;
      }
    }
  }

 private:
  void clean_old_control_timestamps(timestamp_t current_time) {
    // Clean up control trackers
    for (auto& [port, times] : control_time_tracker_) {
      auto it = times.begin();
      while (it != times.end() && it->first < current_time) {
        it = times.erase(it);
      }
    }
  }

  std::optional<size_t> find_port_to_emit(timestamp_t time) {
    size_t active_count = 0;
    std::optional<size_t> selected_port;

    for (size_t port = 0; port < num_control_ports(); ++port) {
      auto it = control_time_tracker_[port].find(time);
      if (it != control_time_tracker_[port].end() && it->second) {
        active_count++;
        selected_port = port;
      }
    }

    return (active_count == 1) ? selected_port : std::nullopt;
  }

  void clean_up_data_messages(timestamp_t time) {
    for (size_t port = 0; port < num_data_ports(); ++port) {
      auto& data_queue = get_data_queue(port);
      while (!data_queue.empty()) {
        auto* msg = dynamic_cast<const Message<T>*>(data_queue.front().get());
        if (msg && msg->time <= time) {
          data_time_tracker_[port].erase(msg->time);
          data_queue.pop_front();
        } else {
          break;
        }
      }
    }
  }

  void clean_up_control_messages(timestamp_t time) {
    // Clean up control queues and trackers
    for (size_t port = 0; port < num_control_ports(); ++port) {
      // Clear all control messages up to and including current time
      auto& control_queue = get_control_queue(port);
      while (!control_queue.empty()) {
        if (auto* msg = dynamic_cast<const Message<BooleanData>*>(control_queue.front().get())) {
          if (msg->time <= time) {
            control_queue.pop_front();
          } else {
            break;
          }
        }
      }

      // Clean up the tracker for this timestamp
      auto& port_tracker = control_time_tracker_[port];
      auto it = port_tracker.begin();
      while (it != port_tracker.end() && it->first <= time) {
        it = port_tracker.erase(it);
      }
    }
  }

  // Track all available timestamps for each data port
  std::map<size_t, std::set<timestamp_t>> data_time_tracker_;

  // Track control values for each port and timestamp
  std::map<size_t, std::map<timestamp_t, bool>> control_time_tracker_;
};

// Factory function for creating a Multiplexer operator
inline std::shared_ptr<Multiplexer<NumberData>> make_multiplexer_number(std::string id, size_t num_ports) {
  return std::make_shared<Multiplexer<NumberData>>(std::move(id), num_ports);
}

inline std::shared_ptr<Multiplexer<BooleanData>> make_multiplexer_boolean(std::string id, size_t num_ports) {
  return std::make_shared<Multiplexer<BooleanData>>(std::move(id), num_ports);
}

inline std::shared_ptr<Multiplexer<VectorNumberData>> make_multiplexer_vector_number(std::string id, size_t num_ports) {
  return std::make_shared<Multiplexer<VectorNumberData>>(std::move(id), num_ports);
}

inline std::shared_ptr<Multiplexer<VectorBooleanData>> make_multiplexer_vector_boolean(std::string id,
                                                                                       size_t num_ports) {
  return std::make_shared<Multiplexer<VectorBooleanData>>(std::move(id), num_ports);
}

}  // namespace rtbot

#endif  // MULTIPLEXER_H