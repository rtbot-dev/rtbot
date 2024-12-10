#ifndef LINEAR_H
#define LINEAR_H

#include "rtbot/Join.h"
#include "rtbot/Message.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Linear : public Join {
 public:
  Linear(std::string id, const std::vector<double>& coeffs)
      : Join(std::move(id), std::vector<std::string>(coeffs.size(), PortType::NUMBER)), coeffs_(coeffs) {
    if (coeffs.size() < 2) {
      throw std::runtime_error("Linear operator requires at least 2 coefficients");
    }
  }

  std::string type_name() const override { return "Linear"; }
  const std::vector<double>& get_coefficients() const { return coeffs_; }

 protected:
  void process_data() override {
    // Call Join's process_data to handle synchronization
    Join::process_data();

    // Get first output queue for reference
    auto& output0 = get_output_queue(0);
    if (output0.empty()) {
      return;
    }

    // Process all synchronized messages
    for (size_t write_index = 0; write_index < output0.size(); write_index++) {
      double result = 0.0;
      timestamp_t time = output0[write_index]->time;

      // Calculate linear combination
      for (size_t i = 0; i < coeffs_.size(); i++) {
        auto& queue = get_output_queue(i);
        if (write_index >= queue.size()) continue;

        const auto* msg = dynamic_cast<const Message<NumberData>*>(queue[write_index].get());
        if (!msg) {
          throw std::runtime_error("Invalid message type in Linear");
        }
        result += coeffs_[i] * msg->data.value;
      }

      // Store result in first output queue
      output0[write_index] = create_message<NumberData>(time, NumberData{result});
    }

    // Clear other output queues
    for (size_t i = 1; i < coeffs_.size(); i++) {
      get_output_queue(i).clear();
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