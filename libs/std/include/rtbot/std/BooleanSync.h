#ifndef BOOLEAN_SYNC_H
#define BOOLEAN_SYNC_H

#include <memory>
#include <optional>

#include "rtbot/Message.h"
#include "rtbot/ReduceJoin.h"

namespace rtbot {

class BooleanSync : public ReduceJoin<BooleanData> {
 public:
  explicit BooleanSync(std::string id, size_t num_ports) : ReduceJoin<BooleanData>(std::move(id), num_ports) {}
  explicit BooleanSync(std::string id, size_t num_ports, bool init_value)
      : ReduceJoin<BooleanData>(std::move(id), num_ports, BooleanData{init_value}) {}

  std::string type_name() const override = 0;
};

class LogicalAnd : public BooleanSync {
 public:
  explicit LogicalAnd(std::string id, size_t num_ports) : BooleanSync(std::move(id), num_ports, true) {}

  std::string type_name() const override { return "LogicalAnd"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& acc, const BooleanData& next) const override {
    return BooleanData{acc.value && next.value};
  }
};

class LogicalOr : public BooleanSync {
 public:
  explicit LogicalOr(std::string id, size_t num_ports) : BooleanSync(std::move(id), num_ports, false) {}

  std::string type_name() const override { return "LogicalOr"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& acc, const BooleanData& next) const override {
    return BooleanData{acc.value || next.value};
  }
};

class LogicalXor : public BooleanSync {
 public:
  explicit LogicalXor(std::string id, size_t num_ports) : BooleanSync(std::move(id), num_ports) {}

  std::string type_name() const override { return "LogicalXor"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& acc, const BooleanData& next) const override {
    return BooleanData{acc.value != next.value};
  }
};

class LogicalNand : public BooleanSync {
 public:
  explicit LogicalNand(std::string id, size_t num_ports) : BooleanSync(std::move(id), num_ports, true) {}

  std::string type_name() const override { return "LogicalNand"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& acc, const BooleanData& next) const override {
    auto temp = acc.value && next.value;
    return BooleanData{!temp};
  }
};

class LogicalNor : public BooleanSync {
 public:
  explicit LogicalNor(std::string id, size_t num_ports) : BooleanSync(std::move(id), num_ports, true) {}

  std::string type_name() const override { return "LogicalNor"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& acc, const BooleanData& next) const override {
    return BooleanData{!(acc.value || next.value)};
  }
};

class LogicalXnor : public BooleanSync {
 public:
  explicit LogicalXnor(std::string id, size_t num_ports) : BooleanSync(std::move(id), num_ports) {}

  std::string type_name() const override { return "LogicalXnor"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& acc, const BooleanData& next) const override {
    return BooleanData{acc.value == next.value};
  }
};

class LogicalImplication : public BooleanSync {
 public:
  explicit LogicalImplication(std::string id, size_t num_ports) : BooleanSync(std::move(id), num_ports, true) {}

  std::string type_name() const override { return "LogicalImplication"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& acc, const BooleanData& next) const override {
    return BooleanData{!acc.value || next.value};
  }
};

// Factory functions
inline std::shared_ptr<LogicalAnd> make_logical_and(std::string id, size_t num_ports = 2) {
  return std::make_shared<LogicalAnd>(std::move(id), num_ports);
}

inline std::shared_ptr<LogicalOr> make_logical_or(std::string id, size_t num_ports = 2) {
  return std::make_shared<LogicalOr>(std::move(id), num_ports);
}

inline std::shared_ptr<LogicalXor> make_logical_xor(std::string id, size_t num_ports = 2) {
  return std::make_shared<LogicalXor>(std::move(id), num_ports);
}

inline std::shared_ptr<LogicalNand> make_logical_nand(std::string id, size_t num_ports = 2) {
  return std::make_shared<LogicalNand>(std::move(id), num_ports);
}

inline std::shared_ptr<LogicalNor> make_logical_nor(std::string id, size_t num_ports = 2) {
  return std::make_shared<LogicalNor>(std::move(id), num_ports);
}

inline std::shared_ptr<LogicalXnor> make_logical_xnor(std::string id, size_t num_ports = 2) {
  return std::make_shared<LogicalXnor>(std::move(id), num_ports);
}

inline std::shared_ptr<LogicalImplication> make_logical_implication(std::string id, size_t num_ports = 2) {
  return std::make_shared<LogicalImplication>(std::move(id), num_ports);
}

}  // namespace rtbot

#endif  // BOOLEAN_SYNC_H