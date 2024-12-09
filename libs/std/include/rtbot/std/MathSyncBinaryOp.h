#ifndef MATH_SYNC_BINARY_OP_H
#define MATH_SYNC_BINARY_OP_H

#include <optional>

#include "rtbot/BinaryJoin.h"
#include "rtbot/Message.h"

namespace rtbot {

template <typename T>
class MathSyncBinaryOp : public BinaryJoin<T> {
 public:
  explicit MathSyncBinaryOp(std::string id) : BinaryJoin<T>(std::move(id)) {}

  std::string type_name() const override = 0;

 protected:
  std::optional<T> combine(const T& a, const T& b) const override = 0;
};

// Concrete implementations
class Addition : public MathSyncBinaryOp<NumberData> {
 public:
  explicit Addition(std::string id) : MathSyncBinaryOp<NumberData>(std::move(id)) {}

  std::string type_name() const override { return "Addition"; }

 protected:
  std::optional<NumberData> combine(const NumberData& a, const NumberData& b) const override {
    return NumberData{a.value + b.value};
  }
};

class Subtraction : public MathSyncBinaryOp<NumberData> {
 public:
  explicit Subtraction(std::string id) : MathSyncBinaryOp<NumberData>(std::move(id)) {}

  std::string type_name() const override { return "Subtraction"; }

 protected:
  std::optional<NumberData> combine(const NumberData& a, const NumberData& b) const override {
    return NumberData{a.value - b.value};
  }
};

class Multiplication : public MathSyncBinaryOp<NumberData> {
 public:
  explicit Multiplication(std::string id) : MathSyncBinaryOp<NumberData>(std::move(id)) {}

  std::string type_name() const override { return "Multiplication"; }

 protected:
  std::optional<NumberData> combine(const NumberData& a, const NumberData& b) const override {
    return NumberData{a.value * b.value};
  }
};

class Division : public MathSyncBinaryOp<NumberData> {
 public:
  explicit Division(std::string id) : MathSyncBinaryOp<NumberData>(std::move(id)) {}

  std::string type_name() const override { return "Division"; }

 protected:
  std::optional<NumberData> combine(const NumberData& a, const NumberData& b) const override {
    if (b.value == 0) {
      return std::nullopt;
    }
    return NumberData{a.value / b.value};
  }
};

// Factory functions
inline std::shared_ptr<Addition> make_addition(std::string id) { return std::make_shared<Addition>(std::move(id)); }

inline std::shared_ptr<Subtraction> make_subtraction(std::string id) {
  return std::make_shared<Subtraction>(std::move(id));
}

inline std::shared_ptr<Multiplication> make_multiplication(std::string id) {
  return std::make_shared<Multiplication>(std::move(id));
}

inline std::shared_ptr<Division> make_division(std::string id) { return std::make_shared<Division>(std::move(id)); }

}  // namespace rtbot

#endif  // MATH_SYNC_BINARY_OP_H