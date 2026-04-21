#ifndef VECTOR_COMPOSE_H
#define VECTOR_COMPOSE_H

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Join.h"
#include "rtbot/Message.h"
#include "rtbot/PortType.h"

namespace rtbot {

class VectorCompose : public Join {
 public:
  VectorCompose(std::string id, size_t num_ports)
      : Join(std::move(id), std::vector<std::string>(num_ports, PortType::NUMBER),
             {PortType::VECTOR_NUMBER},
             /*allow_single_port=*/true),
        num_ports_(num_ports) {
    if (num_ports < 1) {
      throw std::runtime_error("VectorCompose requires at least 1 input port");
    }
  }

  std::string type_name() const override { return "VectorCompose"; }
  size_t get_num_ports() const { return num_ports_; }

  bool equals(const VectorCompose& other) const {
    return num_ports_ == other.num_ports_ && Join::equals(other);
  }

  bool operator==(const VectorCompose& other) const { return equals(other); }
  bool operator!=(const VectorCompose& other) const { return !(*this == other); }

 protected:
  void process_data(bool debug = false) override {
    while (true) {
      // Use the built-in sync mechanism
      bool is_any_empty = false;
      bool is_sync = sync_data_inputs();
      for (size_t i = 0; i < num_ports_; i++) {
        if (get_data_queue(i).empty()) {
          is_any_empty = true;
          break;
        }
      }
      if (!is_sync && is_any_empty) return;
      if (!is_sync) continue;

      // All ports have a message with the same timestamp — concatenate
      VectorNumberData result;
      timestamp_t time = 0;

      for (size_t i = 0; i < num_ports_; i++) {
        const auto* msg = static_cast<const Message<NumberData>*>(get_data_queue(i).front().get());
        if (!msg) {
          throw std::runtime_error("Invalid message type in VectorCompose");
        }
        time = msg->time;
        result.values->push_back(msg->data.value);
      }

      for (size_t i = 0; i < num_ports_; i++) {
        get_data_queue(i).pop_front();
      }

      emit_output(0, create_message<VectorNumberData>(time, result), debug);
    }
  }

 private:
  size_t num_ports_;
};

inline std::shared_ptr<VectorCompose> make_vector_compose(std::string id, size_t num_ports) {
  return std::make_shared<VectorCompose>(std::move(id), num_ports);
}

}  // namespace rtbot

#endif  // VECTOR_COMPOSE_H
