#ifndef REDUCE_JOIN_H
#define REDUCE_JOIN_H

#include "rtbot/Join.h"
#include "rtbot/PortType.h"

namespace rtbot {

template <typename T>
class ReduceJoin : public Join {
 public:
  // Constructor without initial value - single output
  explicit ReduceJoin(std::string id, size_t num_ports)
      : Join(std::move(id), std::vector<std::string>(num_ports, PortType::get_port_type<T>()),  // input ports
             std::vector<std::string>{PortType::get_port_type<T>()})                            // single output port
        ,
        initial_value_(std::nullopt) {
    if (num_ports < 2) {
      throw std::runtime_error("ReduceJoin requires at least 2 input ports");
    }
  }

  // Constructor with initial value - single output
  ReduceJoin(std::string id, size_t num_ports, const T& initial_value)
      : Join(std::move(id), std::vector<std::string>(num_ports, PortType::get_port_type<T>()),  // input ports
             std::vector<std::string>{PortType::get_port_type<T>()})                            // single output port
        ,
        initial_value_(initial_value) {
    if (num_ports < 2) {
      throw std::runtime_error("ReduceJoin requires at least 2 input ports");
    }
  }

  virtual ~ReduceJoin() = default;

  // Pure virtual function to define the reduction operation
  virtual std::optional<T> combine(const T& accumulator, const T& next_value) const = 0;

  bool equals(const ReduceJoin& other) const {
    return (initial_value_ == other.initial_value_ && Operator::equals(other));
  }

 protected:
  void process_data() override {
    
    while(true) {

      bool is_any_empty;
      bool is_sync;
      do {
        is_any_empty = false;
        is_sync = sync_data_inputs();
        for (int i=0; i < num_data_ports(); i++) {
          if (get_data_queue(i).empty()) {
            is_any_empty = true;
            break;
          }
        } 
      } while (!is_sync && !is_any_empty );

      if (!is_sync) return;

      std::vector<const Message<T>*> typed_messages;
      timestamp_t time = 0;
      // Process each synchronized set of messages
      for (int i=0; i < num_data_ports(); i++) {
        typed_messages.reserve(num_data_ports());
        const auto* typed_msg = dynamic_cast<const Message<T>*>(get_data_queue(i).front().get());
        if (!typed_msg) {
          throw std::runtime_error("Invalid message type in ReduceJoin");
        }
        typed_messages.push_back(typed_msg);
        time = typed_msg->time;        
      }

      std::optional<T> result;
      if (initial_value_.has_value()) {
        result = initial_value_;
        for (const auto* msg : typed_messages) {
          result = combine(*result, msg->data);
          if (!result.has_value()) break;
        }
      } else {
        result = typed_messages[0]->data;
        for (size_t i = 1; i < typed_messages.size(); ++i) {
          result = combine(*result, typed_messages[i]->data);
          if (!result.has_value()) break;
        }
      }

      for (int i = 0; i < num_data_ports(); i++)
        get_data_queue(i).pop_front();

      if (result.has_value()) {
        get_output_queue(0).push_back(create_message<T>(time, *result));
      }
    }
  }

 private:
  std::optional<T> initial_value_;
};

}  // namespace rtbot
#endif  // REDUCE_JOIN_H