#ifndef ARITHMETIC_SCALAR_OP_H
#define ARITHMETIC_SCALAR_OP_H

#include <cmath>
#include <functional>
#include <memory>
#include <string>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class ArithmeticScalar : public Operator {
 public:
  ArithmeticScalar(std::string id) : Operator(std::move(id)) {
    // Add single input and output port for numeric data
    add_data_port<NumberData>();
    add_output_port<NumberData>();
  }

  virtual ~ArithmeticScalar() = default;

  // Pure virtual method that derived classes must implement
  virtual double apply(double value) const = 0;

 protected:
  void process_data() override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = dynamic_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in ArithmeticScalar");
      }

      // Apply the mathematical operation and create output message
      output_queue.push_back(create_message<NumberData>(msg->time, NumberData{apply(msg->data.value)}));
      input_queue.pop_front();
    }
  }
};

// Concrete implementations for various mathematical operations
class Add : public ArithmeticScalar {
 public:
  Add(std::string id, double value) : ArithmeticScalar(std::move(id)), value_(value) {}
  std::string type_name() const override { return "Add"; }
  double apply(double x) const override { return x + value_; }
  double get_value() const { return value_; }

 private:
  double value_;
};

class Scale : public ArithmeticScalar {
 public:
  Scale(std::string id, double value) : ArithmeticScalar(std::move(id)), value_(value) {}
  std::string type_name() const override { return "Scale"; }
  double apply(double x) const override { return x * value_; }
  double get_value() const { return value_; }

 private:
  double value_;
};

class Power : public ArithmeticScalar {
 public:
  Power(std::string id, double value) : ArithmeticScalar(std::move(id)), value_(value) {}
  std::string type_name() const override { return "Power"; }
  double apply(double x) const override { return std::pow(x, value_); }
  double get_value() const { return value_; }

 private:
  double value_;
};

// Trigonometric functions
class Sin : public ArithmeticScalar {
 public:
  Sin(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Sin"; }
  double apply(double x) const override { return std::sin(x); }
};

class Cos : public ArithmeticScalar {
 public:
  Cos(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Cos"; }
  double apply(double x) const override { return std::cos(x); }
};

class Tan : public ArithmeticScalar {
 public:
  Tan(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Tan"; }
  double apply(double x) const override { return std::tan(x); }
};

// Exponential and logarithmic functions
class Exp : public ArithmeticScalar {
 public:
  Exp(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Exp"; }
  double apply(double x) const override { return std::exp(x); }
};

class Log : public ArithmeticScalar {
 public:
  Log(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Log"; }
  double apply(double x) const override { return std::log(x); }
};

class Log10 : public ArithmeticScalar {
 public:
  Log10(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Log10"; }
  double apply(double x) const override { return std::log10(x); }
};

// Absolute value and sign functions
class Abs : public ArithmeticScalar {
 public:
  Abs(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Abs"; }
  double apply(double x) const override { return std::abs(x); }
};

class Sign : public ArithmeticScalar {
 public:
  Sign(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Sign"; }
  double apply(double x) const override { return x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0); }
};

// Rounding functions
class Floor : public ArithmeticScalar {
 public:
  Floor(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Floor"; }
  double apply(double x) const override { return std::floor(x); }
};

class Ceil : public ArithmeticScalar {
 public:
  Ceil(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Ceil"; }
  double apply(double x) const override { return std::ceil(x); }
};

class Round : public ArithmeticScalar {
 public:
  Round(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Round"; }
  double apply(double x) const override { return std::round(x); }
};

// Factory functions
inline std::shared_ptr<Add> make_add(std::string id, double value) {
  return std::make_shared<Add>(std::move(id), value);
}

inline std::shared_ptr<Scale> make_scale(std::string id, double value) {
  return std::make_shared<Scale>(std::move(id), value);
}

inline std::shared_ptr<Power> make_power(std::string id, double value) {
  return std::make_shared<Power>(std::move(id), value);
}

inline std::shared_ptr<Sin> make_sin(std::string id) { return std::make_shared<Sin>(std::move(id)); }

inline std::shared_ptr<Cos> make_cos(std::string id) { return std::make_shared<Cos>(std::move(id)); }

inline std::shared_ptr<Tan> make_tan(std::string id) { return std::make_shared<Tan>(std::move(id)); }

inline std::shared_ptr<Exp> make_exp(std::string id) { return std::make_shared<Exp>(std::move(id)); }

inline std::shared_ptr<Log> make_log(std::string id) { return std::make_shared<Log>(std::move(id)); }

inline std::shared_ptr<Log10> make_log10(std::string id) { return std::make_shared<Log10>(std::move(id)); }

inline std::shared_ptr<Abs> make_abs(std::string id) { return std::make_shared<Abs>(std::move(id)); }

inline std::shared_ptr<Sign> make_sign(std::string id) { return std::make_shared<Sign>(std::move(id)); }

inline std::shared_ptr<Floor> make_floor(std::string id) { return std::make_shared<Floor>(std::move(id)); }

inline std::shared_ptr<Ceil> make_ceil(std::string id) { return std::make_shared<Ceil>(std::move(id)); }

inline std::shared_ptr<Round> make_round(std::string id) { return std::make_shared<Round>(std::move(id)); }

}  // namespace rtbot

#endif  // ARITHMETIC_SCALAR_OP_H