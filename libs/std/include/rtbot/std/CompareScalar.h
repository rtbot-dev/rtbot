#ifndef COMPARE_SCALAR_H
#define COMPARE_SCALAR_H

#include <cmath>
#include <memory>
#include <string>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

class CompareScalar : public Operator {
 public:
  CompareScalar(std::string id) : Operator(std::move(id)) {
    add_data_port<NumberData>();
    add_output_port<BooleanData>();
  }

  virtual ~CompareScalar() = default;

  virtual bool evaluate(double value) const = 0;

  bool equals(const CompareScalar& other) const {
    return Operator::equals(other);
  }

 protected:
  void process_data(bool debug = false) override {
    auto& input_queue = get_data_queue(0);
    auto& output_queue = get_output_queue(0);

    while (!input_queue.empty()) {
      const auto* msg = static_cast<const Message<NumberData>*>(input_queue.front().get());
      if (!msg) {
        throw std::runtime_error("Invalid message type in CompareScalar");
      }
      output_queue.push_back(
          create_message<BooleanData>(msg->time, BooleanData{evaluate(msg->data.value)}));
      input_queue.pop_front();
    }
  }
};

class CompareGT : public CompareScalar {
 public:
  CompareGT(std::string id, double value) : CompareScalar(std::move(id)), value_(value) {}
  std::string type_name() const override { return "CompareGT"; }
  bool evaluate(double x) const override { return x > value_; }
  double get_value() const { return value_; }

  bool equals(const CompareGT& other) const {
    return StateSerializer::hash_double(value_) == StateSerializer::hash_double(other.value_) &&
           CompareScalar::equals(other);
  }
  bool operator==(const CompareGT& other) const { return equals(other); }
  bool operator!=(const CompareGT& other) const { return !(*this == other); }

 private:
  double value_;
};

class CompareLT : public CompareScalar {
 public:
  CompareLT(std::string id, double value) : CompareScalar(std::move(id)), value_(value) {}
  std::string type_name() const override { return "CompareLT"; }
  bool evaluate(double x) const override { return x < value_; }
  double get_value() const { return value_; }

  bool equals(const CompareLT& other) const {
    return StateSerializer::hash_double(value_) == StateSerializer::hash_double(other.value_) &&
           CompareScalar::equals(other);
  }
  bool operator==(const CompareLT& other) const { return equals(other); }
  bool operator!=(const CompareLT& other) const { return !(*this == other); }

 private:
  double value_;
};

class CompareGTE : public CompareScalar {
 public:
  CompareGTE(std::string id, double value) : CompareScalar(std::move(id)), value_(value) {}
  std::string type_name() const override { return "CompareGTE"; }
  bool evaluate(double x) const override { return x >= value_; }
  double get_value() const { return value_; }

  bool equals(const CompareGTE& other) const {
    return StateSerializer::hash_double(value_) == StateSerializer::hash_double(other.value_) &&
           CompareScalar::equals(other);
  }
  bool operator==(const CompareGTE& other) const { return equals(other); }
  bool operator!=(const CompareGTE& other) const { return !(*this == other); }

 private:
  double value_;
};

class CompareLTE : public CompareScalar {
 public:
  CompareLTE(std::string id, double value) : CompareScalar(std::move(id)), value_(value) {}
  std::string type_name() const override { return "CompareLTE"; }
  bool evaluate(double x) const override { return x <= value_; }
  double get_value() const { return value_; }

  bool equals(const CompareLTE& other) const {
    return StateSerializer::hash_double(value_) == StateSerializer::hash_double(other.value_) &&
           CompareScalar::equals(other);
  }
  bool operator==(const CompareLTE& other) const { return equals(other); }
  bool operator!=(const CompareLTE& other) const { return !(*this == other); }

 private:
  double value_;
};

class CompareEQ : public CompareScalar {
 public:
  CompareEQ(std::string id, double value, double tolerance = 0.0)
      : CompareScalar(std::move(id)), value_(value), tolerance_(tolerance) {}
  std::string type_name() const override { return "CompareEQ"; }
  bool evaluate(double x) const override { return std::abs(x - value_) <= tolerance_; }
  double get_value() const { return value_; }
  double get_tolerance() const { return tolerance_; }

  bool equals(const CompareEQ& other) const {
    return StateSerializer::hash_double(value_) == StateSerializer::hash_double(other.value_) &&
           StateSerializer::hash_double(tolerance_) == StateSerializer::hash_double(other.tolerance_) &&
           CompareScalar::equals(other);
  }
  bool operator==(const CompareEQ& other) const { return equals(other); }
  bool operator!=(const CompareEQ& other) const { return !(*this == other); }

 private:
  double value_;
  double tolerance_;
};

class CompareNEQ : public CompareScalar {
 public:
  CompareNEQ(std::string id, double value, double tolerance = 0.0)
      : CompareScalar(std::move(id)), value_(value), tolerance_(tolerance) {}
  std::string type_name() const override { return "CompareNEQ"; }
  bool evaluate(double x) const override { return std::abs(x - value_) > tolerance_; }
  double get_value() const { return value_; }
  double get_tolerance() const { return tolerance_; }

  bool equals(const CompareNEQ& other) const {
    return StateSerializer::hash_double(value_) == StateSerializer::hash_double(other.value_) &&
           StateSerializer::hash_double(tolerance_) == StateSerializer::hash_double(other.tolerance_) &&
           CompareScalar::equals(other);
  }
  bool operator==(const CompareNEQ& other) const { return equals(other); }
  bool operator!=(const CompareNEQ& other) const { return !(*this == other); }

 private:
  double value_;
  double tolerance_;
};

// Factory functions
inline std::shared_ptr<CompareGT> make_compare_gt(std::string id, double value) {
  return std::make_shared<CompareGT>(std::move(id), value);
}

inline std::shared_ptr<CompareLT> make_compare_lt(std::string id, double value) {
  return std::make_shared<CompareLT>(std::move(id), value);
}

inline std::shared_ptr<CompareGTE> make_compare_gte(std::string id, double value) {
  return std::make_shared<CompareGTE>(std::move(id), value);
}

inline std::shared_ptr<CompareLTE> make_compare_lte(std::string id, double value) {
  return std::make_shared<CompareLTE>(std::move(id), value);
}

inline std::shared_ptr<CompareEQ> make_compare_eq(std::string id, double value, double tolerance = 0.0) {
  return std::make_shared<CompareEQ>(std::move(id), value, tolerance);
}

inline std::shared_ptr<CompareNEQ> make_compare_neq(std::string id, double value, double tolerance = 0.0) {
  return std::make_shared<CompareNEQ>(std::move(id), value, tolerance);
}

}  // namespace rtbot

#endif  // COMPARE_SCALAR_H
