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

  bool equals(const ArithmeticScalar& other) const {
    return Operator::equals(other);
  }

 protected:
  void process_data(bool debug=false) override {
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

  bool equals(const Add& other) const {
    return (StateSerializer::hash_double(value_) == StateSerializer::hash_double(other.value_) && ArithmeticScalar::equals(other));
  }
  
  bool operator==(const Add& other) const {
    return equals(other);
  }

  bool operator!=(const Add& other) const {
    return !(*this == other);
  }

 private:
  double value_;
};

class Scale : public ArithmeticScalar {
 public:
  Scale(std::string id, double value) : ArithmeticScalar(std::move(id)), value_(value) {}
  std::string type_name() const override { return "Scale"; }
  double apply(double x) const override { return x * value_; }
  double get_value() const { return value_; }

  bool equals(const Scale& other) const {
    return (StateSerializer::hash_double(value_) == StateSerializer::hash_double(other.value_) && ArithmeticScalar::equals(other));
  }
  
  bool operator==(const Scale& other) const {
    return equals(other);
  }

  bool operator!=(const Scale& other) const {
    return !(*this == other);
  }

 private:
  double value_;
};

class Power : public ArithmeticScalar {
 public:
  Power(std::string id, double value) : ArithmeticScalar(std::move(id)), value_(value) {}
  std::string type_name() const override { return "Power"; }
  double apply(double x) const override { return std::pow(x, value_); }
  double get_value() const { return value_; }

  bool equals(const Power& other) const {
    return (StateSerializer::hash_double(value_) == StateSerializer::hash_double(other.value_) && ArithmeticScalar::equals(other));
  }
  
  bool operator==(const Power& other) const {
    return equals(other);
  }

  bool operator!=(const Power& other) const {
    return !(*this == other);
  }

 private:
  double value_;
};

// Trigonometric functions
class Sin : public ArithmeticScalar {
 public:
  Sin(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Sin"; }

  bool equals(const Sin& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Sin& other) const {
    return equals(other);
  }

  bool operator!=(const Sin& other) const {
    return !(*this == other);
  }

  double apply(double x) const override { return std::sin(x); }
};

class Cos : public ArithmeticScalar {
 public:
  Cos(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Cos"; }

  bool equals(const Cos& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Cos& other) const {
    return equals(other);
  }

  bool operator!=(const Cos& other) const {
    return !(*this == other);
  }

  double apply(double x) const override { return std::cos(x); }
};

class Tan : public ArithmeticScalar {
 public:
  Tan(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Tan"; }

  bool equals(const Tan& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Tan& other) const {
    return equals(other);
  }

  bool operator!=(const Tan& other) const {
    return !(*this == other);
  }

  double apply(double x) const override { return std::tan(x); }
};

// Exponential and logarithmic functions
class Exp : public ArithmeticScalar {
 public:
  Exp(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Exp"; }

  bool equals(const Exp& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Exp& other) const {
    return equals(other);
  }

  bool operator!=(const Exp& other) const {
    return !(*this == other);
  }

  double apply(double x) const override { return std::exp(x); }
};

class Log : public ArithmeticScalar {
 public:
  Log(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Log"; }

  bool equals(const Log& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Log& other) const {
    return equals(other);
  }

  bool operator!=(const Log& other) const {
    return !(*this == other);
  }
  
  double apply(double x) const override { return std::log(x); }
};

class Log10 : public ArithmeticScalar {
 public:
  Log10(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Log10"; }

  bool equals(const Log10& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Log10& other) const {
    return equals(other);
  }

  bool operator!=(const Log10& other) const {
    return !(*this == other);
  }

  double apply(double x) const override { return std::log10(x); }
};

// Absolute value and sign functions
class Abs : public ArithmeticScalar {
 public:
  Abs(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Abs"; }

  bool equals(const Abs& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Abs& other) const {
    return equals(other);
  }

  bool operator!=(const Abs& other) const {
    return !(*this == other);
  }

  double apply(double x) const override { return std::abs(x); }
};

class Sign : public ArithmeticScalar {
 public:
  Sign(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Sign"; }

  bool equals(const Sign& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Sign& other) const {
    return equals(other);
  }

  bool operator!=(const Sign& other) const {
    return !(*this == other);
  }

  double apply(double x) const override { return x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0); }
};

// Rounding functions
class Floor : public ArithmeticScalar {
 public:
  Floor(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Floor"; }

  bool equals(const Floor& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Floor& other) const {
    return equals(other);
  }

  bool operator!=(const Floor& other) const {
    return !(*this == other);
  }

  double apply(double x) const override { return std::floor(x); }
};

class Ceil : public ArithmeticScalar {
 public:
  Ceil(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Ceil"; }

  bool equals(const Ceil& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Ceil& other) const {
    return equals(other);
  }

  bool operator!=(const Ceil& other) const {
    return !(*this == other);
  }

  double apply(double x) const override { return std::ceil(x); }
};

class Round : public ArithmeticScalar {
 public:
  Round(std::string id) : ArithmeticScalar(std::move(id)) {}
  std::string type_name() const override { return "Round"; }

  bool equals(const Round& other) const {
    return ArithmeticScalar::equals(other);
  }
  
  bool operator==(const Round& other) const {
    return equals(other);
  }

  bool operator!=(const Round& other) const {
    return !(*this == other);
  }

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