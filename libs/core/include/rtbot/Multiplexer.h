#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

#include <cstddef>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>

#include "Operator.h"
#include "StateSerializer.h"
#include "TimestampTracker.h"

namespace rtbot {

template <typename T>
class Multiplexer : public Operator {
 public:
  Multiplexer(std::string id, size_t num_ports) : Operator(std::move(id)) {
    if (num_ports < 1) {
      throw std::runtime_error("Number of ports must be at least 1");
    }

    // Add data ports
    for (size_t i = 0; i < num_ports; ++i) {
      add_data_port<T>();
    }

    // Add corresponding control ports
    for (size_t i = 0; i < num_ports; ++i) {
      add_control_port<BooleanData>();
    }

    // Single output port
    add_output_port<T>();
  }  

  size_t get_num_ports() const { return data_ports_.size(); }

  std::string type_name() const override { return "Multiplexer"; }


  void receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index) override {
    if (port_index >= num_control_ports()) {
      throw std::runtime_error("Invalid control port index");
    }

    auto* ctrl_msg = dynamic_cast<const Message<BooleanData>*>(msg.get());
    if (!ctrl_msg) {
      throw std::runtime_error("Invalid control message type");
    }
    
    // Update last timestamp
    control_ports_[port_index].last_timestamp = msg->time;

    if (get_control_queue(port_index).size() == max_size_per_port_) {      
      get_control_queue(port_index).pop_front();
    }    
    

    // Add message to queue
    get_control_queue(port_index).push_back(std::move(msg));
    data_ports_with_new_data_.insert(port_index);

  }

 protected:  

  void process_data() override {
    
    while (true) {
      
      int num_empty_data_ports = 0;
      for (int i=0; i < num_data_ports(); i++) {
        if (get_data_queue(i).empty()) {
          num_empty_data_ports++;          
        }
      }
      
      if (num_empty_data_ports == num_data_ports()) return;

      bool is_any_control_empty;
      bool are_control_inputs_sync;
      do {
        is_any_control_empty = false;
        are_control_inputs_sync = sync_control_inputs();
        for (int i=0; i < num_control_ports(); i++) {
          if (get_control_queue(i).empty()) {
            is_any_control_empty = true;
            break;
          }
        } 
      } while (!are_control_inputs_sync && !is_any_control_empty );

      if (!are_control_inputs_sync) return;

      auto* ctrl_msg = dynamic_cast<const Message<BooleanData>*>(get_control_queue(0).front().get());

      int64_t port_to_emit = find_port_to_emit(ctrl_msg->time);
      if (port_to_emit >= 0) {
        bool message_found = false;        
        for (int i = 0; i < num_data_ports(); i++) {
          if (!get_data_queue(i).empty()) {
            auto* msg = dynamic_cast<const Message<T>*>(get_data_queue(i).front().get());
            if (i == port_to_emit && msg->time == ctrl_msg->time) {
              get_output_queue(0).push_back(create_message<T>(msg->time, msg->data));
              get_data_queue(i).pop_front();
              message_found = true;
            } else if (i == port_to_emit && ctrl_msg->time < msg->time) {
              message_found = true;
            } else if (msg->time <= ctrl_msg->time) {
              get_data_queue(i).pop_front();              
            }
          }
        }
        if (message_found) {
          for (int i = 0; i < num_control_ports(); i++) {
            get_control_queue(i).pop_front();            
          }
        }
      } else {
        clean_data_input_queue_fronts(ctrl_msg->time);
        for (int i = 0; i < num_control_ports(); i++) {
          get_control_queue(i).pop_front();            
        }
      }
      
    }
  }

 private:
 
  void clean_data_input_queue_fronts(timestamp_t time) {
    for (int i = 0; i < num_data_ports(); i++) {
      if (!get_data_queue(i).empty()) {
        auto* msg = dynamic_cast<const Message<T>*>(get_data_queue(i).front().get());
        if (msg && msg->time <= time) get_data_queue(i).pop_front();        
      }
    }
  }

  int64_t find_port_to_emit(timestamp_t time) {
    size_t active_count = 0;
    int64_t selected_port = -1;

    for (size_t i = 0; i < num_control_ports(); i++) {
      auto* ctrl_msg = dynamic_cast<const Message<BooleanData>*>(get_control_queue(i).front().get());
      if ((ctrl_msg->time == time) && ctrl_msg->data.value) {
        active_count++;
        selected_port = static_cast<int64_t>(i);
      }
    }

    return (active_count == 1) ? selected_port : -1;
  }   
};

// Factory function for creating a Multiplexer operator
inline std::shared_ptr<Multiplexer<NumberData>> make_multiplexer_number(std::string id, size_t num_ports) {
  return std::make_shared<Multiplexer<NumberData>>(std::move(id), num_ports);
}

inline std::shared_ptr<Multiplexer<BooleanData>> make_multiplexer_boolean(std::string id, size_t num_ports) {
  return std::make_shared<Multiplexer<BooleanData>>(std::move(id), num_ports);
}

inline std::shared_ptr<Multiplexer<VectorNumberData>> make_multiplexer_vector_number(std::string id, size_t num_ports) {
  return std::make_shared<Multiplexer<VectorNumberData>>(std::move(id), num_ports);
}

inline std::shared_ptr<Multiplexer<VectorBooleanData>> make_multiplexer_vector_boolean(std::string id,
                                                                                       size_t num_ports) {
  return std::make_shared<Multiplexer<VectorBooleanData>>(std::move(id), num_ports);
}

}  // namespace rtbot

#endif  // MULTIPLEXER_H