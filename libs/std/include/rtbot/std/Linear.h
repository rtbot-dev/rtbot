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

 protected:
  void process_data() override {
    sync();
    const auto& synced_data = get_synchronized_data();

    for (const auto& [time, messages] : synced_data) {
      std::vector<const Message<NumberData>*> typed_messages;
      typed_messages.reserve(messages.size());

      for (const auto& msg : messages) {
        const auto* typed_msg = dynamic_cast<const Message<NumberData>*>(msg.get());
        if (!typed_msg) {
          throw std::runtime_error("Invalid message type in Linear");
        }
        typed_messages.push_back(typed_msg);
      }

      double result = 0.0;
      for (size_t i = 0; i < coeffs_.size(); i++) {
        result += coeffs_[i] * typed_messages[i]->data.value;
      }

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