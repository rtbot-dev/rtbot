#ifndef JOIN_H
#define JOIN_H

#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"
#include "rtbot/TimestampTracker.h"

namespace rtbot {

class Join : public Operator {
 public:
  // Base constructor with separate input and output port specifications
  Join(std::string id, const std::vector<std::string>& input_port_types,
       const std::vector<std::string>& output_port_types)
      : Operator(std::move(id)) {
    if (input_port_types.size() < 2) {
      throw std::runtime_error("Join requires at least 2 input ports");
    }

    // Add input ports and initialize tracking
    for (const auto& type : input_port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown port type: " + type);
      }

      PortType::add_port(*this, type, true, false, false);  // input only      
      port_type_names_.push_back(type);
    }

    // Add output ports
    for (const auto& type : output_port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown port type: " + type);
      }
      PortType::add_port(*this, type,false ,false, true);  // output only
    }
  }

  // Constructor with port types only
  Join(std::string id, const std::vector<std::string>& port_types) : Operator(std::move(id)) {
    if (port_types.size() < 2) {
      throw std::runtime_error("Join requires at least 2 input ports");
    }

    for (const auto& type : port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown port type: " + type);
      }

      PortType::add_port(*this, type, true, false ,true);      
      port_type_names_.push_back(type);
    }
  }

  // Constructor with number of ports of the same type
  template <typename T>
  Join(std::string id, size_t num_ports) : Operator(std::move(id)) {
    if (num_ports < 2) {
      throw std::runtime_error("Join requires at least 2 input ports");
    }

    std::string port_type = PortType::get_port_type<T>();
    for (size_t i = 0; i < num_ports; ++i) {
      PortType::add_port(*this, port_type, true, false ,true);      
      port_type_names_.push_back(port_type);
    }
  }

  std::string type_name() const override { return "Join"; }

  // Get port configuration
  const std::vector<std::string>& get_port_types() const { return port_type_names_; }

  bool equals(const Join& other) const {
    return (port_type_names_ == other.port_type_names_ && Operator::equals(other));
  }

  bool operator==(const Join& other) const {
    return equals(other);
  }

  bool operator!=(const Join& other) const {
    return !(*this == other);
  }

 protected:
  // Performs synchronization of input messages

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

      for (int i=0; i < num_data_ports(); i++) {
        get_output_queue(i).push_back(get_data_queue(i).front()->clone());
        get_data_queue(i).pop_front();
      }

    }
  }


 private:
  std::vector<std::string> port_type_names_;
};

// Factory functions remain unchanged
inline std::shared_ptr<Join> make_join(std::string id, const std::vector<std::string>& port_types) {
  return std::make_shared<Join>(std::move(id), port_types);
}

template <typename T>
inline std::shared_ptr<Join> make_binary_join(std::string id) {
  return std::make_shared<Join>(std::move(id), 2);
}

inline std::shared_ptr<Join> make_number_join(std::string id, size_t num_ports) {
  return std::make_shared<Join>(std::move(id), std::vector<std::string>(num_ports, PortType::NUMBER));
}

inline std::shared_ptr<Join> make_boolean_join(std::string id, size_t num_ports) {
  return std::make_shared<Join>(std::move(id), std::vector<std::string>(num_ports, PortType::BOOLEAN));
}

}  // namespace rtbot

#endif  // JOIN_H