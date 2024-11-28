#ifndef DIVISION_H
#define DIVISION_H

#include <iostream>

#include "rtbot/BinaryJoin.h"
#include "rtbot/Message.h"

namespace rtbot {

class Division : public BinaryJoin<NumberData> {
 public:
  Division(std::string id) : BinaryJoin<NumberData>(std::move(id)) {}

  std::string type_name() const override { return "Division"; }

 protected:
  std::optional<NumberData> combine(const NumberData& a, const NumberData& b) const override {
    if (b.value == 0.0) {
      return std::nullopt;
    }
    return NumberData{a.value / b.value};
  }
};

}  // namespace rtbot

#endif  // DIVISION_H