#ifndef BINARY_JOIN_H
#define BINARY_JOIN_H

#include <iostream>

#include "rtbot/Join.h"
#include "rtbot/PortType.h"

namespace rtbot {

template <typename T>
class BinaryJoin : public Join {
 public:
  explicit BinaryJoin(std::string id)
      : Join(std::move(id), std::vector<std::string>{PortType::get_port_type<T>(), PortType::get_port_type<T>()}) {}

  // Pure virtual function to make the class abstract
  virtual std::optional<T> combine(const T& a, const T& b) const = 0;

 protected:
  void process_data() override {
    // First let Join handle synchronization
    Join::process_data();

    // Get references to output queues
    auto& output0 = get_output_queue(0);
    auto& output1 = get_output_queue(1);

    // Process all synchronized pairs
    size_t write_index = 0;
    while (write_index < output0.size()) {
      const auto* msg1 = dynamic_cast<const Message<T>*>(output0[write_index].get());
      const auto* msg2 = dynamic_cast<const Message<T>*>(output1[write_index].get());

      if (!msg1 || !msg2) {
        throw std::runtime_error("Invalid message type in BinaryJoin");
      }

      // Apply the binary operation
      auto result = combine(msg1->data, msg2->data);

      if (result.has_value()) {
        // Replace the message in output0 with our combined result
        output0[write_index] = create_message<T>(msg1->time, *result);
        write_index++;
      } else {
        // If the operation failed, remove the message from output0
        output0.erase(output0.begin() + write_index);
      }
    }

    // Clear output1 as we're done with it
    output1.clear();
  }
};

}  // namespace rtbot

#endif  // BINARY_JOIN_H