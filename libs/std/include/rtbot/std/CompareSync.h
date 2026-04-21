#ifndef COMPARE_SYNC_H
#define COMPARE_SYNC_H

#include <cmath>
#include <memory>
#include <string>

#include "rtbot/Join.h"
#include "rtbot/Message.h"
#include "rtbot/PortType.h"

namespace rtbot {

// CompareSync: synchronizes two NumberData streams by timestamp and emits
// evaluate(lhs, rhs) as BooleanData. Inherits timestamp-sync machinery from Join.
class CompareSync : public Join {
 public:
  explicit CompareSync(std::string id)
      : Join(std::move(id),
             {PortType::NUMBER, PortType::NUMBER},   // i1, i2
             {PortType::BOOLEAN}) {}                 // o1

  virtual ~CompareSync() = default;

  virtual bool evaluate(double lhs, double rhs) const = 0;

  bool equals(const CompareSync& other) const {
    return Join::equals(other);
  }

 protected:
  void process_data(bool debug = false) override {
    while (true) {
      bool is_any_empty;
      bool is_sync;
      do {
        is_any_empty = false;
        is_sync = sync_data_inputs();
        for (int i = 0; i < num_data_ports(); i++) {
          if (get_data_queue(i).empty()) {
            is_any_empty = true;
            break;
          }
        }
      } while (!is_sync && !is_any_empty);

      if (!is_sync) return;

      const auto* msg1 =
          static_cast<const Message<NumberData>*>(get_data_queue(0).front().get());
      const auto* msg2 =
          static_cast<const Message<NumberData>*>(get_data_queue(1).front().get());
      if (!msg1 || !msg2) {
        throw std::runtime_error("Invalid message type in CompareSync");
      }

      auto time = msg1->time;
      bool result = evaluate(msg1->data.value, msg2->data.value);
      emit_output(0,
          create_message<BooleanData>(time, BooleanData{result}), debug);

      get_data_queue(0).pop_front();
      get_data_queue(1).pop_front();
    }
  }
};

class CompareSyncGT : public CompareSync {
 public:
  explicit CompareSyncGT(std::string id)
      : CompareSync(std::move(id)) {}
  std::string type_name() const override { return "CompareSyncGT"; }
  bool evaluate(double lhs, double rhs) const override { return lhs > rhs; }

  bool equals(const CompareSyncGT& other) const { return CompareSync::equals(other); }
  bool operator==(const CompareSyncGT& other) const { return equals(other); }
  bool operator!=(const CompareSyncGT& other) const { return !(*this == other); }
};

class CompareSyncLT : public CompareSync {
 public:
  explicit CompareSyncLT(std::string id)
      : CompareSync(std::move(id)) {}
  std::string type_name() const override { return "CompareSyncLT"; }
  bool evaluate(double lhs, double rhs) const override { return lhs < rhs; }

  bool equals(const CompareSyncLT& other) const { return CompareSync::equals(other); }
  bool operator==(const CompareSyncLT& other) const { return equals(other); }
  bool operator!=(const CompareSyncLT& other) const { return !(*this == other); }
};

class CompareSyncGTE : public CompareSync {
 public:
  explicit CompareSyncGTE(std::string id)
      : CompareSync(std::move(id)) {}
  std::string type_name() const override { return "CompareSyncGTE"; }
  bool evaluate(double lhs, double rhs) const override { return lhs >= rhs; }

  bool equals(const CompareSyncGTE& other) const { return CompareSync::equals(other); }
  bool operator==(const CompareSyncGTE& other) const { return equals(other); }
  bool operator!=(const CompareSyncGTE& other) const { return !(*this == other); }
};

class CompareSyncLTE : public CompareSync {
 public:
  explicit CompareSyncLTE(std::string id)
      : CompareSync(std::move(id)) {}
  std::string type_name() const override { return "CompareSyncLTE"; }
  bool evaluate(double lhs, double rhs) const override { return lhs <= rhs; }

  bool equals(const CompareSyncLTE& other) const { return CompareSync::equals(other); }
  bool operator==(const CompareSyncLTE& other) const { return equals(other); }
  bool operator!=(const CompareSyncLTE& other) const { return !(*this == other); }
};

class CompareSyncEQ : public CompareSync {
 public:
  explicit CompareSyncEQ(std::string id, double tolerance = 0.0)
      : CompareSync(std::move(id)), tolerance_(tolerance) {}
  std::string type_name() const override { return "CompareSyncEQ"; }
  bool evaluate(double lhs, double rhs) const override {
    return std::abs(lhs - rhs) <= tolerance_;
  }
  double get_tolerance() const { return tolerance_; }

  bool equals(const CompareSyncEQ& other) const {
    return StateSerializer::hash_double(tolerance_) ==
               StateSerializer::hash_double(other.tolerance_) &&
           CompareSync::equals(other);
  }
  bool operator==(const CompareSyncEQ& other) const { return equals(other); }
  bool operator!=(const CompareSyncEQ& other) const { return !(*this == other); }

 private:
  double tolerance_;
};

class CompareSyncNEQ : public CompareSync {
 public:
  explicit CompareSyncNEQ(std::string id, double tolerance = 0.0)
      : CompareSync(std::move(id)), tolerance_(tolerance) {}
  std::string type_name() const override { return "CompareSyncNEQ"; }
  bool evaluate(double lhs, double rhs) const override {
    return std::abs(lhs - rhs) > tolerance_;
  }
  double get_tolerance() const { return tolerance_; }

  bool equals(const CompareSyncNEQ& other) const {
    return StateSerializer::hash_double(tolerance_) ==
               StateSerializer::hash_double(other.tolerance_) &&
           CompareSync::equals(other);
  }
  bool operator==(const CompareSyncNEQ& other) const { return equals(other); }
  bool operator!=(const CompareSyncNEQ& other) const { return !(*this == other); }

 private:
  double tolerance_;
};

// Factory functions
inline std::shared_ptr<CompareSyncGT>  make_compare_sync_gt(std::string id) {
  return std::make_shared<CompareSyncGT>(std::move(id));
}
inline std::shared_ptr<CompareSyncLT>  make_compare_sync_lt(std::string id) {
  return std::make_shared<CompareSyncLT>(std::move(id));
}
inline std::shared_ptr<CompareSyncGTE> make_compare_sync_gte(std::string id) {
  return std::make_shared<CompareSyncGTE>(std::move(id));
}
inline std::shared_ptr<CompareSyncLTE> make_compare_sync_lte(std::string id) {
  return std::make_shared<CompareSyncLTE>(std::move(id));
}
inline std::shared_ptr<CompareSyncEQ>  make_compare_sync_eq(std::string id,
                                                             double tolerance = 0.0) {
  return std::make_shared<CompareSyncEQ>(std::move(id), tolerance);
}
inline std::shared_ptr<CompareSyncNEQ> make_compare_sync_neq(std::string id,
                                                              double tolerance = 0.0) {
  return std::make_shared<CompareSyncNEQ>(std::move(id), tolerance);
}

}  // namespace rtbot

#endif  // COMPARE_SYNC_H
