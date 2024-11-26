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

  // Pure virtual function to make the class abstract
  virtual std::optional<T> combine(const T& a, const T& b) const = 0;

 protected:
  void process_data() override {
    // First let Join handle synchronization
    Join::process_data();

    // Check if synchronization produced any output
    const auto& output0 = get_output_queue(0);
    const auto& output1 = get_output_queue(1);

    if (!output0.empty() && !output1.empty()) {
      // Get synchronized messages
      const auto* msg1 = dynamic_cast<const Message<T>*>(output0.front().get());
      const auto* msg2 = dynamic_cast<const Message<T>*>(output1.front().get());

      if (!msg1 || !msg2) {
        throw std::runtime_error("Invalid message type in BinaryJoin");
      }

      // Apply the binary operation
      auto result = combine(msg1->data, msg2->data);

      // Clear both output queues
      get_output_queue(0).clear();
      get_output_queue(1).clear();

      if (result) {
        // Create new output message on port 0
        auto out_msg = create_message<T>(msg1->time, *result);
        get_output_queue(0).push_back(std::move(out_msg));
      }
    }
  }
};

}  // namespace rtbot

#endif  // BINARY_JOIN_H