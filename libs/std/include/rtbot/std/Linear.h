#ifndef LINEAR_H
#define LINEAR_H

#include "rtbot/Join.h"
#include "rtbot/Message.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Linear : public Join {
 public:
  Linear(std::string id, const std::vector<double>& coeffs)
      : Join(std::move(id), std::vector<std::string>(coeffs.size(), PortType::NUMBER),  // input ports
             std::vector<std::string>{PortType::NUMBER})                                // single output port
        ,
        coeffs_(coeffs) {
    if (coeffs.size() < 2) {
      throw std::runtime_error("Linear operator requires at least 2 coefficients");
    }
  }

  std::string type_name() const override { return "Linear"; }
  const std::vector<double>& get_coefficients() const { return coeffs_; }

  bool equals(const Linear& other) const {
    return (coeffs_ == other.coeffs_ && Join::equals(other));
  }

  bool operator==(const Linear& other) const {
    return equals(other);
  }

  bool operator!=(const Linear& other) const {
    return !(*this == other);
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

      std::vector<const Message<NumberData>*> typed_messages;
      timestamp_t time = 0;
      // Process each synchronized set of messages
      for (int i=0; i < num_data_ports(); i++) {
        typed_messages.reserve(num_data_ports());
        const auto* typed_msg = dynamic_cast<const Message<NumberData>*>(get_data_queue(i).front().get());
        if (!typed_msg) {
          throw std::runtime_error("Invalid message type in Linear");
        }
        typed_messages.push_back(typed_msg);
        time = typed_msg->time;        
      }

      double result = 0.0;
      for (size_t i = 0; i < coeffs_.size(); i++) {
        result += coeffs_[i] * typed_messages[i]->data.value;
      }

      for (int i = 0; i < num_data_ports(); i++)
        get_data_queue(i).pop_front();

      get_output_queue(0).push_back(create_message<NumberData>(time, NumberData{result}));
    }
  }

 private:
  std::vector<double> coeffs_;
};

// Factory function
inline std::shared_ptr<Linear> make_linear(std::string id, const std::vector<double>& coeffs) {
  return std::make_shared<Linear>(std::move(id), coeffs);
}

}  // namespace rtbot

#endif  // LINEAR_H