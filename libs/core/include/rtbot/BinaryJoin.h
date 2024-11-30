#ifndef BINARY_JOIN_H
#define BINARY_JOIN_H

#include "rtbot/Join.h"
#include "rtbot/PortType.h"

namespace rtbot {

template <typename T>
class BinaryJoin : public Join {
 public:
  explicit BinaryJoin(std::string id)
      : Join(std::move(id), std::vector<std::string>{PortType::get_port_type<T>(), PortType::get_port_type<T>()}) {}

  virtual ~BinaryJoin() = default;

  // Pure virtual function to define the binary operation
  virtual std::optional<T> combine(const T& a, const T& b) const = 0;

 protected:
  void process_data() override {
    Join::process_data();  // Let base class handle synchronization

    if (get_output_queue(0).empty() || get_output_queue(1).empty()) {
      return;  // No synchronized messages to process
    }

    auto& output0 = get_output_queue(0);
    auto& output1 = get_output_queue(1);

    size_t write_index = 0;
    while (write_index < output0.size() && write_index < output1.size()) {
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
        output0.erase(output0.begin() + write_index);
      }
    }

    // Truncate output0 to match what we processed
    if (write_index < output0.size()) {
      output0.resize(write_index);
    }

    // Clear second output as we're done with it
    output1.clear();
  }
};

}  // namespace rtbot

#endif  // BINARY_JOIN_H