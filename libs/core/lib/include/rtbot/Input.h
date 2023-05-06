#ifndef INPUT_H
#define INPUT_H

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "rtbot/Buffer.h"

namespace rtbot {

template <class T, class V>
struct Input : public Buffer<T,V> {
  static const int size = 2;

  Input() = default;

  Input(string const &id_) : Buffer<T,V>(id_, Input<T,V>::size) {}

  string typeName() const override { return "Input"; }

  map<string, std::vector<Message<T,V>>> processData() override {
    if (this->at(1).time <= this->at(0).time) return {};
    return this->emit(this->at(0));
  }
};

}  // namespace rtbot

#endif  // INPUT_H
