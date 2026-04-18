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

  bool equals(const Identity& other) const {
    return Operator::equals(other);
  }

  bool operator==(const Identity& other) const {
      return equals(other);
  }

  bool operator!=(const Identity& other) const {
      return !(*this == other);
  }

 protected:
  void process_data(bool debug=false) override {
    auto& input_queue = get_data_queue(0);
    if (input_queue.empty()) return;
    if (input_queue.size() >= kEmitBatchThreshold) {
      std::vector<std::unique_ptr<BaseMessage>> batch;
      batch.reserve(input_queue.size());
      while (!input_queue.empty()) {
        batch.push_back(std::move(input_queue.front()));
        input_queue.pop_front();
      }
      emit_output(0, std::move(batch), debug);
    } else {
      while (!input_queue.empty()) {
        emit_output(0, std::move(input_queue.front()), debug);
        input_queue.pop_front();
      }
    }
  }
};

// Factory function
inline std::shared_ptr<Identity> make_identity(std::string id) { return std::make_shared<Identity>(std::move(id)); }

}  // namespace rtbot

#endif  // IDENTITY_H