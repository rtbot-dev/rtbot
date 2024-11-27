#ifndef AND_H
#define AND_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

class And : public BinaryJoin<BooleanData> {
 public:
  explicit And(std::string id) : BinaryJoin(std::move(id)) {}

  std::string type_name() const override { return "And"; }

 protected:
  std::optional<BooleanData> combine(const BooleanData& a, const BooleanData& b) const override {
    return BooleanData{a.value && b.value};
  }
};

}  // namespace rtbot

#endif  // AND_H