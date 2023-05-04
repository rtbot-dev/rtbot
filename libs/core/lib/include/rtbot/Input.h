#ifndef INPUT_H
#define INPUT_H

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "rtbot/Buffer.h"

namespace rtbot {

template <class T = double>
struct Input : public Buffer<T> {
  static const int size = 2;

  Input() = default;

  Input(string const &id_) : Buffer<T>(id_, Input::size) {}

  string typeName() const override { return "Input"; }

  map<string, std::vector<Message<T>>> processData() override {
    if ((std::int64_t)(this->at(1).time - this->at(0).time) <= 0) return {};
    return this->emit(this->at(0));
  }
};

}  // namespace rtbot

#endif  // INPUT_H
