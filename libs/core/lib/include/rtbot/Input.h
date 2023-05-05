#ifndef INPUT_H
#define INPUT_H

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "rtbot/Buffer.h"

namespace rtbot {

template <class V = double>
struct Input : public Buffer<V> {
  static const int size = 2;

  Input() = default;

  Input(string const &id_) : Buffer<V>(id_, Input::size) {}

  string typeName() const override { return "Input"; }

  map<string, std::vector<Message<V>>> processData() override {
    if (this->at(1).time <= this->at(0).time) return {};
    return this->emit(this->at(0));
  }
};

}  // namespace rtbot

#endif  // INPUT_H
