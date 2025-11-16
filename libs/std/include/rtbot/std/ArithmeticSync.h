#ifndef ARITHMETIC_SYNC_H
#define ARITHMETIC_SYNC_H

#include <optional>

#include "rtbot/Message.h"
#include "rtbot/ReduceJoin.h"

namespace rtbot {

template <typename T>
class ArithmeticSync : public ReduceJoin<T> {
 public:
  explicit ArithmeticSync(std::string id, size_t num_ports) : ReduceJoin<T>(std::move(id), num_ports) {}

  explicit ArithmeticSync(std::string id, size_t num_ports, const T& init_value)
      : ReduceJoin<T>(std::move(id), num_ports, init_value) {}

  std::string type_name() const override = 0;

  bool equals(const ArithmeticSync& other) const {
    return ReduceJoin<T>::equals(other);
  }

};

class Addition : public ArithmeticSync<NumberData> {
 public:
  explicit Addition(std::string id, size_t num_ports)
      : ArithmeticSync<NumberData>(std::move(id), num_ports, NumberData{0.0}) {}

  std::string type_name() const override { return "Addition"; }

  bool equals(const Addition& other) const {
    return ArithmeticSync::equals(other);
  }
  
  bool operator==(const Addition& other) const {
    return equals(other);
  }

  bool operator!=(const Addition& other) const {
    return !(*this == other);
  }

 protected:
  std::optional<NumberData> combine(const NumberData& acc, const NumberData& next) const override {
    return NumberData{acc.value + next.value};
  }
};

class Subtraction : public ArithmeticSync<NumberData> {
 public:
  explicit Subtraction(std::string id, size_t num_ports = 2) : ArithmeticSync<NumberData>(std::move(id), num_ports) {}

  std::string type_name() const override { return "Subtraction"; }

  bool equals(const Subtraction& other) const {
    return ArithmeticSync::equals(other);
  }
  
  bool operator==(const Subtraction& other) const {
    return equals(other);
  }

  bool operator!=(const Subtraction& other) const {
    return !(*this == other);
  }

 protected:
  std::optional<NumberData> combine(const NumberData& acc, const NumberData& next) const override {
    // For 2 inputs, maintain original behavior
    if (this->num_data_ports() == 2) {
      return NumberData{acc.value - next.value};
    }
    // For multiple inputs, subtract from first value
    return NumberData{acc.value - next.value};
  }
};

class Multiplication : public ArithmeticSync<NumberData> {
 public:
  explicit Multiplication(std::string id, size_t num_ports)
      : ArithmeticSync<NumberData>(std::move(id), num_ports, NumberData{1.0}) {}

  std::string type_name() const override { return "Multiplication"; }

  bool equals(const Multiplication& other) const {
    return ArithmeticSync::equals(other);
  }
  
  bool operator==(const Multiplication& other) const {
    return equals(other);
  }

  bool operator!=(const Multiplication& other) const {
    return !(*this == other);
  }

 protected:
  std::optional<NumberData> combine(const NumberData& acc, const NumberData& next) const override {
    return NumberData{acc.value * next.value};
  }
};

class Division : public ArithmeticSync<NumberData> {
 public:
  explicit Division(std::string id, size_t num_ports = 2) : ArithmeticSync<NumberData>(std::move(id), num_ports) {}

  std::string type_name() const override { return "Division"; }

  bool equals(const Division& other) const {
    return ArithmeticSync::equals(other);
  }
  
  bool operator==(const Division& other) const {
    return equals(other);
  }

  bool operator!=(const Division& other) const {
    return !(*this == other);
  }

 protected:
  std::optional<NumberData> combine(const NumberData& acc, const NumberData& next) const override {
    if (next.value == 0) {
      return std::nullopt;
    }

    // For 2 inputs, maintain original behavior
    if (this->num_data_ports() == 2) {
      return NumberData{acc.value / next.value};
    }

    // For multiple inputs, divide first value by product of others
    return NumberData{acc.value / next.value};
  }
};

// Factory functions with optional num_ports parameter
inline std::shared_ptr<Addition> make_addition(std::string id, size_t num_ports = 2) {
  return std::make_shared<Addition>(std::move(id), num_ports);
}

inline std::shared_ptr<Subtraction> make_subtraction(std::string id, size_t num_ports = 2) {
  return std::make_shared<Subtraction>(std::move(id), num_ports);
}

inline std::shared_ptr<Multiplication> make_multiplication(std::string id, size_t num_ports = 2) {
  return std::make_shared<Multiplication>(std::move(id), num_ports);
}

inline std::shared_ptr<Division> make_division(std::string id, size_t num_ports = 2) {
  return std::make_shared<Division>(std::move(id), num_ports);
}

}  // namespace rtbot

#endif  // ARITHMETIC_SYNC_H