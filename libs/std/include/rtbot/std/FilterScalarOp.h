#ifndef FILTER_SCALAR_OP_H
#define FILTER_SCALAR_OP_H

#include <functional>
#include <memory>
#include <string>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class FilterScalarOp : public Operator {
 public:
  FilterScalarOp(std::string id) : Operator(std::move(id)) {
    // Add single input and output port for numeric data
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  virtual ~FilterScalarOp() = default;

  // Pure virtual method that derived classes must implement
  virtual bool evaluate(double value) const = 0;

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in FilterScalarOp");
      }

      // Forward message only if condition evaluates to true
      if (evaluate(msg->data.value)) {
        output_queue.push_back(input_queue.front()->clone());
      }

      input_queue.pop_front();
    }
  }
};

// Concrete implementations for various filter operations
class LessThan : public FilterScalarOp {
 public:
  LessThan(std::string id, double threshold) : FilterScalarOp(std::move(id)), threshold_(threshold) {}
  std::string type_name() const override { return "LessThan"; }
  bool evaluate(double x) const override { return x < threshold_; }
  double get_threshold() const { return threshold_; }

 private:
  double threshold_;
};

class GreaterThan : public FilterScalarOp {
 public:
  GreaterThan(std::string id, double threshold) : FilterScalarOp(std::move(id)), threshold_(threshold) {}
  std::string type_name() const override { return "GreaterThan"; }
  bool evaluate(double x) const override { return x > threshold_; }
  double get_threshold() const { return threshold_; }

 private:
  double threshold_;
};

class EqualTo : public FilterScalarOp {
 public:
  EqualTo(std::string id, double value, double epsilon = 1e-10)
      : FilterScalarOp(std::move(id)), value_(value), epsilon_(epsilon) {}

  std::string type_name() const override { return "EqualTo"; }
  bool evaluate(double x) const override { return std::abs(x - value_) <= epsilon_; }
  double get_value() const { return value_; }
  double get_epsilon() const { return epsilon_; }

 private:
  double value_;
  double epsilon_;  // Tolerance for floating-point comparison
};

class NotEqualTo : public FilterScalarOp {
 public:
  NotEqualTo(std::string id, double value, double epsilon = 1e-10)
      : FilterScalarOp(std::move(id)), value_(value), epsilon_(epsilon) {}

  std::string type_name() const override { return "NotEqualTo"; }
  bool evaluate(double x) const override { return std::abs(x - value_) > epsilon_; }
  double get_value() const { return value_; }
  double get_epsilon() const { return epsilon_; }

 private:
  double value_;
  double epsilon_;  // Tolerance for floating-point comparison
};

// Factory functions
inline std::shared_ptr<LessThan> make_less_than(std::string id, double threshold) {
  return std::make_shared<LessThan>(std::move(id), threshold);
}

inline std::shared_ptr<GreaterThan> make_greater_than(std::string id, double threshold) {
  return std::make_shared<GreaterThan>(std::move(id), threshold);
}

inline std::shared_ptr<EqualTo> make_equal_to(std::string id, double value, double epsilon = 1e-10) {
  return std::make_shared<EqualTo>(std::move(id), value, epsilon);
}

}  // namespace rtbot

#endif  // FILTER_SCALAR_OP_H