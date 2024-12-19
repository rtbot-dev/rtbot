#ifndef FILTER_SYNC_H
#define FILTER_SYNC_H

#include "rtbot/ReduceJoin.h"

namespace rtbot {

template <typename T>
class FilterSync : public ReduceJoin<T> {
 public:
  explicit FilterSync(std::string id, size_t num_ports) : ReduceJoin<T>(std::move(id), num_ports) {}
  virtual bool filter_condition(const T& first, const T& acc) const = 0;

 protected:
  std::optional<T> combine(const T& acc, const T& next) const override {
    return filter_condition(acc, next) ? std::optional<T>(acc) : std::nullopt;
  }
};

class SyncGreaterThan : public FilterSync<NumberData> {
 public:
  explicit SyncGreaterThan(std::string id, size_t num_ports = 2) : FilterSync<NumberData>(std::move(id), num_ports) {}

  std::string type_name() const override { return "SyncGreaterThan"; }

  bool filter_condition(const NumberData& first, const NumberData& acc) const override {
    return first.value > acc.value;
  }
};

class SyncLessThan : public FilterSync<NumberData> {
 public:
  explicit SyncLessThan(std::string id, size_t num_ports = 2) : FilterSync<NumberData>(std::move(id), num_ports) {}

  std::string type_name() const override { return "SyncLessThan"; }

  bool filter_condition(const NumberData& first, const NumberData& acc) const override {
    return first.value < acc.value;
  }
};

class SyncEqual : public FilterSync<NumberData> {
 public:
  explicit SyncEqual(std::string id, size_t num_ports = 2, double epsilon = 1e-10)
      : FilterSync<NumberData>(std::move(id), num_ports), epsilon_(epsilon) {
    if (epsilon <= 0.0) {
      throw std::runtime_error("Epsilon must be positive");
    }
  }

  std::string type_name() const override { return "SyncEqual"; }

  bool filter_condition(const NumberData& first, const NumberData& acc) const override {
    return std::abs(first.value - acc.value) < epsilon_;
  }

  double get_epsilon() const { return epsilon_; }

 private:
  double epsilon_;
};

class SyncNotEqual : public FilterSync<NumberData> {
 public:
  explicit SyncNotEqual(std::string id, size_t num_ports = 2, double epsilon = 1e-10)
      : FilterSync<NumberData>(std::move(id), num_ports), epsilon_(epsilon) {
    if (epsilon <= 0.0) {
      throw std::runtime_error("Epsilon must be positive");
    }
  }

  std::string type_name() const override { return "SyncNotEqual"; }

  bool filter_condition(const NumberData& first, const NumberData& acc) const override {
    return std::abs(first.value - acc.value) >= epsilon_;
  }

  double get_epsilon() const { return epsilon_; }

 private:
  double epsilon_;
};

// Factory functions
inline std::shared_ptr<SyncGreaterThan> make_sync_greater_than(std::string id, size_t num_ports = 2) {
  return std::make_shared<SyncGreaterThan>(std::move(id), num_ports);
}

inline std::shared_ptr<SyncLessThan> make_sync_less_than(std::string id, size_t num_ports = 2) {
  return std::make_shared<SyncLessThan>(std::move(id), num_ports);
}

inline std::shared_ptr<SyncEqual> make_sync_equal(std::string id, size_t num_ports = 2, double epsilon = 1e-10) {
  return std::make_shared<SyncEqual>(std::move(id), num_ports, epsilon);
}

inline std::shared_ptr<SyncNotEqual> make_sync_not_equal(std::string id, size_t num_ports = 2, double epsilon = 1e-10) {
  return std::make_shared<SyncNotEqual>(std::move(id), num_ports, epsilon);
}

}  // namespace rtbot

#endif  // FILTER_SYNC_H