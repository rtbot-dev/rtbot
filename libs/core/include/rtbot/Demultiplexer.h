#ifndef DEMULTIPLEXER_H
#define DEMULTIPLEXER_H

#include <cstddef>

#include "StateSerializer.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"
#include "rtbot/TimestampTracker.h"

namespace rtbot {

template <typename T>
class Demultiplexer : public Operator {
 public:
  Demultiplexer(std::string id, size_t num_ports) : Operator(std::move(id)) {
    if (num_ports < 1) {
      throw std::runtime_error("Number of output ports must be at least 1");
    }

    // Add single data input port with type T
    add_data_port<T>();    

    // Add corresponding control ports (always boolean)
    for (size_t i = 0; i < num_ports; ++i) {
      add_control_port<BooleanData>();      
    }

    // Add output ports (same type as input)
    for (size_t i = 0; i < num_ports; ++i) {
      add_output_port<T>();
    }
  }

  std::string type_name() const override { return "Demultiplexer"; }

  size_t get_num_ports() const { return num_control_ports(); }

  bool equals(const Demultiplexer& other) const {
    return Operator::equals(other);
  }
  
  bool operator==(const Demultiplexer& other) const {
    return equals(other);
  }

  bool operator!=(const Demultiplexer& other) const {
    return !(*this == other);
  }

 protected:  

  void process_data() override {

    while(true) {

      bool is_any_control_empty;
      bool are_controls_sync;
      do {
        is_any_control_empty = false;
        are_controls_sync = sync_control_inputs();
        for (int i=0; i < num_control_ports(); i++) {
          if (get_control_queue(i).empty()) {
            is_any_control_empty = true;
            break;
          }
        } 
      } while (!are_controls_sync && !is_any_control_empty );

      if (!are_controls_sync) return;

      auto& data_queue = get_data_queue(0);
      if (data_queue.empty()) return;
      auto* msg = dynamic_cast<const Message<T>*>(data_queue.front().get());
      auto* ctrl_msg = dynamic_cast<const Message<BooleanData>*>(get_control_queue(0).front().get());
      if (msg && ctrl_msg && msg->time == ctrl_msg->time) {
        for (int i = 0; i < num_control_ports(); i++) {
          ctrl_msg = dynamic_cast<const Message<BooleanData>*>(get_control_queue(i).front().get());          
          if (ctrl_msg->data.value) {
            get_output_queue(i).push_back(data_queue.front()->clone());
          }
          get_control_queue(i).pop_front();
        }
        data_queue.pop_front();
      } else if (msg && ctrl_msg && msg->time < ctrl_msg->time) {
        data_queue.pop_front();        
      } else if (msg && ctrl_msg && ctrl_msg->time < msg->time) {
        for (int i = 0; i < num_control_ports(); i++)
          get_control_queue(i).pop_front();
        
      }
    }    
  }  

};

// Factory functions for common configurations using PortType
inline std::shared_ptr<Demultiplexer<NumberData>> make_demultiplexer_number(std::string id, size_t num_ports) {
  return std::make_shared<Demultiplexer<NumberData>>(std::move(id), num_ports);
}

inline std::shared_ptr<Demultiplexer<BooleanData>> make_demultiplexer_boolean(std::string id, size_t num_ports) {
  return std::make_shared<Demultiplexer<BooleanData>>(std::move(id), num_ports);
}

inline std::shared_ptr<Demultiplexer<VectorNumberData>> make_demultiplexer_vector_number(std::string id,
                                                                                         size_t num_ports) {
  return std::make_shared<Demultiplexer<VectorNumberData>>(std::move(id), num_ports);
}

inline std::shared_ptr<Demultiplexer<VectorBooleanData>> make_demultiplexer_vector_boolean(std::string id,
                                                                                           size_t num_ports) {
  return std::make_shared<Demultiplexer<VectorBooleanData>>(std::move(id), num_ports);
}

}  // namespace rtbot

#endif  // DEMULTIPLEXER_H