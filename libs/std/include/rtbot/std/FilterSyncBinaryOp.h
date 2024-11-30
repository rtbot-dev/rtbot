#ifndef FILTER_SYNC_BINARY_OP_H
#define FILTER_SYNC_BINARY_OP_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

template <typename T>
class FilterSyncBinaryOp : public BinaryJoin<T> {
 public:
  explicit FilterSyncBinaryOp(std::string id) : BinaryJoin<T>(std::move(id)) {}

  virtual bool filter_condition(const T& a, const T& b) const = 0;

 protected:
  std::optional<T> combine(const T& a, const T& b) const override {
    return filter_condition(a, b) ? std::optional<T>(a) : std::nullopt;
  }
};

class SyncGreaterThan : public FilterSyncBinaryOp<NumberData> {
 public:
  explicit SyncGreaterThan(std::string id) : FilterSyncBinaryOp<NumberData>(std::move(id)) {}
  std::string type_name() const override { return "SyncGreaterThan"; }
  bool filter_condition(const NumberData& a, const NumberData& b) const override { return a.value > b.value; }
};

class SyncLessThan : public FilterSyncBinaryOp<NumberData> {
 public:
  explicit SyncLessThan(std::string id) : FilterSyncBinaryOp<NumberData>(std::move(id)) {}
  std::string type_name() const override { return "SyncLessThan"; }
  bool filter_condition(const NumberData& a, const NumberData& b) const override { return a.value < b.value; }
};

class SyncEqual : public FilterSyncBinaryOp<NumberData> {
 public:
  explicit SyncEqual(std::string id, double epsilon = 1e-10)
      : FilterSyncBinaryOp<NumberData>(std::move(id)), epsilon_(epsilon) {
    if (epsilon <= 0.0) {
      throw std::runtime_error("Epsilon must be positive");
    }
  }

  std::string type_name() const override { return "SyncEqual"; }

  bool filter_condition(const NumberData& a, const NumberData& b) const override {
    return std::abs(a.value - b.value) < epsilon_;
  }

  double get_epsilon() const { return epsilon_; }

 private:
  double epsilon_;
};

}  // namespace rtbot

#endif  // FILTER_SYNC_BINARY_OP_H