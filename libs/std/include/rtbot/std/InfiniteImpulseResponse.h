#ifndef INFINITE_IMPULSE_RESPONSE_H
#define INFINITE_IMPULSE_RESPONSE_H

#include <deque>
#include <stdexcept>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class InfiniteImpulseResponse : public Operator {
 public:
  InfiniteImpulseResponse(std::string id, std::vector<double> b_coeffs, std::vector<double> a_coeffs)
      : Operator(std::move(id)), b_(b_coeffs), a_(a_coeffs) {
    if (b_coeffs.empty() || a_coeffs.empty()) {
      throw std::runtime_error("Both coefficient vectors must have at least one element");
    }

    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  std::string type_name() const override { return "InfiniteImpulseResponse"; }

  std::vector<double> get_a_coeffs() const { return a_; }
  std::vector<double> get_b_coeffs() const { return b_; }

  Bytes collect() override {
    Bytes bytes = Operator::collect();

    // Serialize x buffer
    size_t x_size = x_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&x_size),
                 reinterpret_cast<const uint8_t*>(&x_size) + sizeof(x_size));
    for (const auto& x : x_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&x), reinterpret_cast<const uint8_t*>(&x) + sizeof(x));
    }

    // Serialize y buffer
    size_t y_size = y_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&y_size),
                 reinterpret_cast<const uint8_t*>(&y_size) + sizeof(y_size));
    for (const auto& y : y_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&y), reinterpret_cast<const uint8_t*>(&y) + sizeof(y));
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    // Restore x buffer
    size_t x_size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);
    x_.clear();
    for (size_t i = 0; i < x_size; ++i) {
      double value = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      x_.push_back(value);
    }

    // Restore y buffer
    size_t y_size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);
    y_.clear();
    for (size_t i = 0; i < y_size; ++i) {
      double value = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      y_.push_back(value);
    }
  }

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in IIR filter");
      }

      // Add input to buffer
      x_.push_back(msg->data.value);

      // Keep buffer size limited
      if (x_.size() > b_.size()) {
        x_.pop_front();
      }

      // Calculate new output if we have enough inputs
      if (x_.size() == b_.size()) {
        // Calculate new output
        double y_n = 0.0;

        // Add input terms (b coefficients)
        for (size_t i = 0; i < b_.size(); ++i) {
          y_n += b_[i] * x_[x_.size() - 1 - i];
        }

        // Subtract available output terms (a coefficients)
        size_t available_outputs = std::min(y_.size(), a_.size());
        for (size_t i = 0; i < available_outputs; ++i) {
          y_n -= a_[i] * y_[y_.size() - 1 - i];
        }

        // Update output buffer
        y_.push_back(y_n);

        if (y_.size() > a_.size()) {
          y_.pop_front();
        }

        // Create output message
        output_queue.push_back(create_message<NumberData>(msg->time, NumberData{y_n}));
      }

      input_queue.pop_front();
    }
  }

 private:
  std::vector<double> b_;  // Input coefficients
  std::vector<double> a_;  // Output coefficients
  std::deque<double> x_;   // Input buffer
  std::deque<double> y_;   // Output buffer
};

inline std::shared_ptr<Operator> make_iir(const std::string& id, std::vector<double> b_coeffs,
                                          std::vector<double> a_coeffs) {
  return std::make_shared<InfiniteImpulseResponse>(id, b_coeffs, a_coeffs);
}

}  // namespace rtbot

#endif  // INFINITE_IMPULSE_RESPONSE_H