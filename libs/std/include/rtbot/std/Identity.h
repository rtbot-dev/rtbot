#ifndef IDENTITY_H
#define IDENTITY_H

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Identity : public Operator {
 public:
  Identity(std::string id) : Operator(std::move(id)) {
    // Single input and output port
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "Identity"; }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in Identity");
      }

      // Forward message by cloning
      output_queue.push_back(input_queue.front()->clone());
      input_queue.pop_front();
    }
  }
};

// Factory function
inline std::shared_ptr<Identity> make_identity(std::string id) { return std::make_shared<Identity>(std::move(id)); }

}  // namespace rtbot

#endif  // IDENTITY_H