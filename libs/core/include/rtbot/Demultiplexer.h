#ifndef DEMULTIPLEXER_H
#define DEMULTIPLEXER_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"
#include "rtbot/TimestampTracker.h"

namespace rtbot {

template <typename T>
class Demultiplexer : public Operator {
 public:
  Demultiplexer(std::string id, size_t num_ports) : Operator(std::move(id)) {
    if (num_ports < 1) {
      throw std::runtime_error("Number of output ports must be at least 1");
    }

    // Add single data input port with type T
    add_data_port<T>();
    data_time_tracker_ = std::set<timestamp_t>();

    // Add corresponding control ports (always boolean)
    for (size_t i = 0; i < num_ports; ++i) {
      add_control_port<BooleanData>();
      control_time_tracker_[i] = std::map<timestamp_t, bool>();
    }

    // Add output ports (same type as input)
    for (size_t i = 0; i < num_ports; ++i) {
      add_output_port<T>();
    }
  }

  void restore(Bytes::const_iterator& it) override {
    // Clear current state
    data_time_tracker_.clear();
    control_time_tracker_.clear();

    // Restore data time tracker
    size_t data_times_size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    for (size_t i = 0; i < data_times_size; ++i) {
      timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
      it += sizeof(timestamp_t);
      data_time_tracker_.insert(time);
    }

    // Restore control time tracker
    size_t num_ports = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    for (size_t i = 0; i < num_ports; ++i) {
      size_t times_size = *reinterpret_cast<const size_t*>(&(*it));
      it += sizeof(size_t);

      auto& port_times = control_time_tracker_[i];
      for (size_t j = 0; j < times_size; ++j) {
        timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
        it += sizeof(timestamp_t);

        bool value = *reinterpret_cast<const bool*>(&(*it));
        it += sizeof(bool);

        port_times[time] = value;
      }
    }
  }

  Bytes collect() override {
    Bytes bytes;

    // Save data time tracker
    size_t data_times_size = data_time_tracker_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&data_times_size),
                 reinterpret_cast<const uint8_t*>(&data_times_size) + sizeof(data_times_size));

    for (const auto& time : data_time_tracker_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                   reinterpret_cast<const uint8_t*>(&time) + sizeof(time));
    }

    // Save control time tracker
    size_t num_ports = control_time_tracker_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&num_ports),
                 reinterpret_cast<const uint8_t*>(&num_ports) + sizeof(num_ports));

    for (const auto& [port, times] : control_time_tracker_) {
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

  void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    if (port_index >= num_data_ports()) {
      throw std::runtime_error("Invalid data port index");
    }

    auto* data_msg = dynamic_cast<const Message<T>*>(msg.get());
    if (!data_msg) {
      throw std::runtime_error("Invalid data message type");
    }

    // Add timestamp to tracker
    data_time_tracker_.insert(data_msg->time);

    // Add message to queue
    get_data_queue(port_index).push_back(std::move(msg));
    data_ports_with_new_data_.insert(port_index);
  }

  void receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    if (port_index >= num_control_ports()) {
      throw std::runtime_error("Invalid control port index");
    }

    auto* ctrl_msg = dynamic_cast<const Message<BooleanData>*>(msg.get());
    if (!ctrl_msg) {
      throw std::runtime_error("Invalid control message type");
    }

    // Update control tracker
    control_time_tracker_[port_index][ctrl_msg->time] = ctrl_msg->data.value;

    // Add message to queue
    get_control_queue(port_index).push_back(std::move(msg));
    control_ports_with_new_data_.insert(port_index);
  }

 protected:
  void process_data() override {
    while (true) {
      // Find oldest common control timestamp
      auto common_control_time = TimestampTracker::find_oldest_common_time(control_time_tracker_);
      if (!common_control_time) {
        break;
      }

      // Clean up any old input data messages
      auto& data_queue = get_data_queue(0);
      while (!data_queue.empty()) {
        auto* msg = dynamic_cast<const Message<T>*>(data_queue.front().get());
        if (msg && msg->time < *common_control_time) {
          data_time_tracker_.erase(msg->time);
          data_queue.pop_front();
        } else {
          break;
        }
      }

      // Look for matching data message
      bool message_found = false;
      if (!data_queue.empty()) {
        auto* msg = dynamic_cast<const Message<T>*>(data_queue.front().get());
        if (msg && msg->time == *common_control_time) {
          // Count active control ports
          std::vector<size_t> active_ports;
          for (size_t i = 0; i < num_control_ports(); ++i) {
            if (control_time_tracker_[i].at(*common_control_time)) {
              active_ports.push_back(i);
            }
          }

          // Route message if exactly one control is active
          if (active_ports.size() == 1) {
            get_output_queue(active_ports[0]).push_back(data_queue.front()->clone());
          } else if (active_ports.size() > 1) {
            throw std::runtime_error("Multiple control ports active at the same time");
          } else {
            throw std::runtime_error("No control port active at the same time");
          }

          data_time_tracker_.erase(msg->time);
          data_queue.pop_front();
          message_found = true;
        }
      }

      clean_up_control_messages(*common_control_time);

      if (!message_found) {
        break;
      }
    }
  }

 private:
  void clean_up_control_messages(timestamp_t time) {
    for (auto& [port, tracker] : control_time_tracker_) {
      tracker.erase(time);
    }

    for (size_t port = 0; port < num_control_ports(); ++port) {
      auto& queue = get_control_queue(port);
      while (!queue.empty()) {
        auto* msg = dynamic_cast<const Message<BooleanData>*>(queue.front().get());
        if (msg && msg->time <= time) {
          queue.pop_front();
        } else {
          break;
        }
      }
    }
  }

  std::set<timestamp_t> data_time_tracker_;
  std::map<size_t, std::map<timestamp_t, bool>> control_time_tracker_;
};

// Factory functions for common configurations using PortType
inline std::unique_ptr<Demultiplexer<NumberData>> make_number_demultiplexer(std::string id, size_t num_ports) {
  return std::make_unique<Demultiplexer<NumberData>>(std::move(id), num_ports);
}

inline std::unique_ptr<Demultiplexer<BooleanData>> make_boolean_demultiplexer(std::string id, size_t num_ports) {
  return std::make_unique<Demultiplexer<BooleanData>>(std::move(id), num_ports);
}

inline std::unique_ptr<Demultiplexer<VectorNumberData>> make_vector_number_demultiplexer(std::string id,
                                                                                         size_t num_ports) {
  return std::make_unique<Demultiplexer<VectorNumberData>>(std::move(id), num_ports);
}

inline std::unique_ptr<Demultiplexer<VectorBooleanData>> make_vector_boolean_demultiplexer(std::string id,
                                                                                           size_t num_ports) {
  return std::make_unique<Demultiplexer<VectorBooleanData>>(std::move(id), num_ports);
}

}  // namespace rtbot

#endif  // DEMULTIPLEXER_H