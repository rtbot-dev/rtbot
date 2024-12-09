#ifndef BOOLEAN_SYNC_BINARY_OP_H
#define BOOLEAN_SYNC_BINARY_OP_H

#include <memory>
#include <optional>

#include "rtbot/BinaryJoin.h"
#include "rtbot/Message.h"

namespace rtbot {

class BooleanSyncBinaryOp : public BinaryJoin<BooleanData> {
 public:
  explicit BooleanSyncBinaryOp(std::string id) : BinaryJoin<BooleanData>(std::move(id)) {}

  std::string type_name() const override = 0;
};

// AND operator
class LogicalAnd : public BooleanSyncBinaryOp {
 public:
  explicit LogicalAnd(std::string id) : BooleanSyncBinaryOp(std::move(id)) {}

  std::string type_name() const override { return "LogicalAnd"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& a, const BooleanData& b) const override {
    return BooleanData{a.value && b.value};
  }
};

// OR operator
class LogicalOr : public BooleanSyncBinaryOp {
 public:
  explicit LogicalOr(std::string id) : BooleanSyncBinaryOp(std::move(id)) {}

  std::string type_name() const override { return "LogicalOr"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& a, const BooleanData& b) const override {
    return BooleanData{a.value || b.value};
  }
};

// XOR operator
class LogicalXor : public BooleanSyncBinaryOp {
 public:
  explicit LogicalXor(std::string id) : BooleanSyncBinaryOp(std::move(id)) {}

  std::string type_name() const override { return "LogicalXor"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& a, const BooleanData& b) const override {
    return BooleanData{a.value != b.value};
  }
};

// NAND operator
class LogicalNand : public BooleanSyncBinaryOp {
 public:
  explicit LogicalNand(std::string id) : BooleanSyncBinaryOp(std::move(id)) {}

  std::string type_name() const override { return "LogicalNand"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& a, const BooleanData& b) const override {
    return BooleanData{!(a.value && b.value)};
  }
};

// NOR operator
class LogicalNor : public BooleanSyncBinaryOp {
 public:
  explicit LogicalNor(std::string id) : BooleanSyncBinaryOp(std::move(id)) {}

  std::string type_name() const override { return "LogicalNor"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& a, const BooleanData& b) const override {
    return BooleanData{!(a.value || b.value)};
  }
};

// XNOR operator (equivalence)
class LogicalXnor : public BooleanSyncBinaryOp {
 public:
  explicit LogicalXnor(std::string id) : BooleanSyncBinaryOp(std::move(id)) {}

  std::string type_name() const override { return "LogicalXnor"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& a, const BooleanData& b) const override {
    return BooleanData{a.value == b.value};
  }
};

// Implication operator (a → b)
class LogicalImplication : public BooleanSyncBinaryOp {
 public:
  explicit LogicalImplication(std::string id) : BooleanSyncBinaryOp(std::move(id)) {}

  std::string type_name() const override { return "LogicalImplication"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& a, const BooleanData& b) const override {
    return BooleanData{!a.value || b.value};
  }
};

// Factory functions
inline std::shared_ptr<LogicalAnd> make_logical_and(std::string id) {
  return std::make_shared<LogicalAnd>(std::move(id));
}

inline std::shared_ptr<LogicalOr> make_logical_or(std::string id) { return std::make_shared<LogicalOr>(std::move(id)); }

inline std::shared_ptr<LogicalXor> make_logical_xor(std::string id) {
  return std::make_shared<LogicalXor>(std::move(id));
}

inline std::shared_ptr<LogicalNand> make_logical_nand(std::string id) {
  return std::make_shared<LogicalNand>(std::move(id));
}

inline std::shared_ptr<LogicalNor> make_logical_nor(std::string id) {
  return std::make_shared<LogicalNor>(std::move(id));
}

inline std::shared_ptr<LogicalXnor> make_logical_xnor(std::string id) {
  return std::make_shared<LogicalXnor>(std::move(id));
}

inline std::shared_ptr<LogicalImplication> make_logical_implication(std::string id) {
  return std::make_shared<LogicalImplication>(std::move(id));
}

}  // namespace rtbot

#endif  // BOOLEAN_SYNC_BINARY_OP_H