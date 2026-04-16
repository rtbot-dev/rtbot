#ifndef COLLECTOR_H
#define COLLECTOR_H

#include <string>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

// Sink operator that accumulates messages in its input queues for external
// reading. Connect it as a child of any operator whose output you want to
// inspect (tests, Program output collection, etc.).
class Collector : public Operator {
 public:
  Collector(std::string id, const std::vector<std::string>& port_types) : Operator(std::move(id)) {
    if (port_types.empty()) {
      throw std::runtime_error("Collector must have at least one port type");
    }
    for (const auto& type : port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Invalid port type: " + type);
      }
      PortType::add_port(*this, type, true, false, false);
      port_type_names_.push_back(type);
    }
  }

  std::string type_name() const override { return "Collector"; }

  bool is_sink() const override { return true; }

  const std::vector<std::string>& get_port_types() const { return port_type_names_; }

  // Override to accept messages without timestamp ordering or queue-size limits —
  // Collector is a sink that buffers everything its parent emits for external inspection.
  void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index, bool /*debug*/ = false) override {
    if (port_index >= num_data_ports()) {
      throw std::runtime_error("Invalid data port index at Collector(" + id() + "):" +
                               std::to_string(port_index));
    }
    get_data_queue(port_index).push_back(std::move(msg));
  }

  void receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index, bool /*debug*/ = false) override {
    if (port_index >= num_control_ports()) {
      throw std::runtime_error("Invalid control port index at Collector(" + id() + "):" +
                               std::to_string(port_index));
    }
    get_control_queue(port_index).push_back(std::move(msg));
  }

 protected:
  void process_data(bool debug = false) override {}

 private:
  std::vector<std::string> port_type_names_;
};

inline std::shared_ptr<Collector> make_collector(std::string id, const std::vector<std::string>& port_types) {
  return std::make_shared<Collector>(std::move(id), port_types);
}

inline std::shared_ptr<Collector> make_number_collector(std::string id) {
  return std::make_shared<Collector>(std::move(id), std::vector<std::string>{PortType::NUMBER});
}

inline std::shared_ptr<Collector> make_boolean_collector(std::string id) {
  return std::make_shared<Collector>(std::move(id), std::vector<std::string>{PortType::BOOLEAN});
}

inline std::shared_ptr<Collector> make_vector_number_collector(std::string id) {
  return std::make_shared<Collector>(std::move(id), std::vector<std::string>{PortType::VECTOR_NUMBER});
}

}  // namespace rtbot

#endif  // COLLECTOR_H
