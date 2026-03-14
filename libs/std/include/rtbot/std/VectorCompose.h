#ifndef VECTOR_COMPOSE_H
#define VECTOR_COMPOSE_H

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class VectorCompose : public Operator {
 public:
  VectorCompose(std::string id, size_t num_ports) : Operator(std::move(id)), num_ports_(num_ports) {
    if (num_ports < 1) {
      throw std::runtime_error("VectorCompose requires at least 1 input port");
    }
    // SQL compiler emits scalar NumberData streams and uses VectorCompose to build
    // a row vector. Each input port therefore consumes NumberData.
    for (size_t i = 0; i < num_ports; ++i) {
      add_data_port<NumberData>();
    }
    add_output_port<VectorNumberData>();
  }

  std::string type_name() const override { return "VectorCompose"; }
  size_t get_num_ports() const { return num_ports_; }

  bool equals(const VectorCompose& other) const {
    return num_ports_ == other.num_ports_ && Operator::equals(other);
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
        const auto* msg = dynamic_cast<const Message<NumberData>*>(get_data_queue(i).front().get());
        if (!msg) {
          throw std::runtime_error("Invalid message type in VectorCompose");
        }
        time = msg->time;
        result.values.push_back(msg->data.value);
      }

      for (size_t i = 0; i < num_ports_; i++) {
        get_data_queue(i).pop_front();
      }

      get_output_queue(0).push_back(create_message<VectorNumberData>(time, result));
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
