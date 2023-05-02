#ifndef INPUT_H
#define INPUT_H

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "rtbot/Buffer.h"

namespace rtbot {

struct Input : public Buffer<double> {
  static const int size = 2;

  Input() = default;

  Input(string const &id_) : Buffer<double>(id_, Input::size) {}

  string typeName() const override { return "Input"; }

  map<string, std::vector<Message<>>> processData() override {
    if ((std::int64_t)(at(1).time - at(0).time) <= 0) return {};
    return emit(at(0));
  }
};

}  // namespace rtbot

#endif  // INPUT_H
