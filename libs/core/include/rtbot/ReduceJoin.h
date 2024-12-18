#ifndef REDUCE_JOIN_H
#define REDUCE_JOIN_H

#include "rtbot/Join.h"
#include "rtbot/PortType.h"

namespace rtbot {

template <typename T>
class ReduceJoin : public Join {
 public:
  // Constructor without initial value
  explicit ReduceJoin(std::string id, size_t num_ports)
      : Join(std::move(id), std::vector<std::string>(num_ports, PortType::get_port_type<T>())),
        initial_value_(std::nullopt) {
    if (num_ports < 2) {
      throw std::runtime_error("ReduceJoin requires at least 2 input ports");
    }
  }

  // Constructor with initial value
  ReduceJoin(std::string id, size_t num_ports, const T& initial_value)
      : Join(std::move(id), std::vector<std::string>(num_ports, PortType::get_port_type<T>())),
        initial_value_(initial_value) {
    if (num_ports < 2) {
      throw std::runtime_error("ReduceJoin requires at least 2 input ports");
    }
  }

  virtual ~ReduceJoin() = default;

  // Pure virtual function to define the reduction operation
  virtual std::optional<T> combine(const T& accumulator, const T& next_value) const = 0;

 protected:
  void process_data() override {
    Join::process_data();  // Let base class handle synchronization

    // Check if we have synchronized messages across all ports
    size_t num_ports = num_data_ports();
    std::vector<MessageQueue*> output_queues;
    output_queues.reserve(num_ports);

    size_t min_queue_size = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < num_ports; ++i) {
      auto& queue = get_output_queue(i);
      if (queue.empty()) {
        return;  // No synchronized messages to process
      }
      min_queue_size = std::min(min_queue_size, queue.size());
      output_queues.push_back(&queue);
    }

    // Process synchronized messages using reduction
    size_t write_index = 0;
    while (write_index < min_queue_size) {
      // Collect synchronized messages
      std::vector<const Message<T>*> messages;
      messages.reserve(num_ports);

      for (size_t i = 0; i < num_ports; ++i) {
        const auto* msg = dynamic_cast<const Message<T>*>((*output_queues[i])[write_index].get());
        if (!msg) {
          throw std::runtime_error("Invalid message type in ReduceJoin " + type_name() + "(" + id() + ")");
        }
        messages.push_back(msg);
      }

      // Perform reduction
      std::optional<T> result;

      if (initial_value_.has_value()) {
        // If we have an initial value, use it
        result = initial_value_;
      } else {
        // Otherwise, start with the first message's value
        result = messages[0]->data;
        messages.erase(messages.begin());  // Skip first message in reduction
      }

      // Reduce remaining messages
      for (const auto* msg : messages) {
        result = combine(*result, msg->data);
        if (!result.has_value()) {
          break;  // Stop reduction if combine returns nullopt
        }
      }

      if (result.has_value()) {
        // Store result in first output queue
        (*output_queues[0])[write_index] = create_message<T>(messages[0]->time, *result);
        write_index++;
      } else {
        // Remove this set of messages if reduction failed
        for (auto* queue : output_queues) {
          queue->erase(queue->begin() + write_index);
        }
      }
    }

    // Truncate output0 to match what we processed
    auto& output0 = get_output_queue(0);
    if (write_index < output0.size()) {
      output0.resize(write_index);
    }

    // Clear all output queues except the first one (which contains our results)
    for (size_t i = 1; i < num_ports; ++i) {
      output_queues[i]->clear();
    }
  }

 private:
  std::optional<T> initial_value_;
};

}  // namespace rtbot
#endif  // REDUCE_JOIN_H