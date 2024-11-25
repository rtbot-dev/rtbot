#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>

#include "Operator.h"
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

  // State serialization
  Bytes collect() override {
    Bytes bytes;

    // Save data time tracker
    size_t data_ports = data_time_tracker_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&data_ports),
                 reinterpret_cast<const uint8_t*>(&data_ports) + sizeof(data_ports));

    for (const auto& [port, times] : data_time_tracker_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port),
                   reinterpret_cast<const uint8_t*>(&port) + sizeof(port));

      size_t times_size = times.size();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&times_size),
                   reinterpret_cast<const uint8_t*>(&times_size) + sizeof(times_size));

      for (const auto& time : times) {
        bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                     reinterpret_cast<const uint8_t*>(&time) + sizeof(time));
      }
    }

    // Save control time tracker
    size_t num_ports = control_time_tracker_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&num_ports),
                 reinterpret_cast<const uint8_t*>(&num_ports) + sizeof(num_ports));

    for (const auto& [port, times] : control_time_tracker_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port),
                   reinterpret_cast<const uint8_t*>(&port) + sizeof(port));

      size_t times_size = times.size();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&times_size),
                   reinterpret_cast<const uint8_t*>(&times_size) + sizeof(times_size));

      for (const auto& [time, value] : times) {
        bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                     reinterpret_cast<const uint8_t*>(&time) + sizeof(time));
        bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&value),
                     reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
      }
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    // Clear current state
    data_time_tracker_.clear();
    control_time_tracker_.clear();

    // Restore data time tracker
    size_t data_ports = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    for (size_t i = 0; i < data_ports; ++i) {
      size_t port = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);

      size_t times_size = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);

      auto& port_times = data_time_tracker_[port];
      for (size_t j = 0; j < times_size; ++j) {
        timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
        it += sizeof(timestamp_t);
        port_times.insert(time);
      }
    }

    // Restore control time tracker
    size_t num_ports = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    for (size_t i = 0; i < num_ports; ++i) {
      size_t port = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);

      size_t times_size = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);

      auto& port_times = control_time_tracker_[port];
      for (size_t j = 0; j < times_size; ++j) {
        timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
        it += sizeof(timestamp_t);

        bool value = *reinterpret_cast<const bool*>(&(*it));
        it += sizeof(bool);

        port_times[time] = value;
      }
    }
  }

  void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    if (port_index >= num_data_ports()) {
      throw std::runtime_error("Invalid data port index");
    }

    auto* data_msg = dynamic_cast<const Message<T>*>(msg.get());
    if (!data_msg) {
      throw std::runtime_error("Invalid data message type");
    }

    // Add timestamp to the tracker
    data_time_tracker_[port_index].insert(data_msg->time);

    // Add message to queue
    get_data_queue(port_index).push_back(std::move(msg));
    ports_with_new_data_.insert(port_index);
  }

  void receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    if (port_index >= num_control_ports()) {
      throw std::runtime_error("Invalid control port index");
    }

    auto* ctrl_msg = dynamic_cast<const Message<BooleanData>*>(msg.get());
    if (!ctrl_msg) {
      throw std::runtime_error("Invalid control message type");
    }

    // Update control time tracker
    control_time_tracker_[port_index][ctrl_msg->time] = ctrl_msg->data.value;

    // Add message to queue
    get_control_queue(port_index).push_back(std::move(msg));
    ports_with_new_data_.insert(port_index);
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

}  // namespace rtbot

#endif  // MULTIPLEXER_H