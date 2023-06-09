#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include <cstdint>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Accumulator : public Operator<T, V> {
  static const size_t size = 1;
  V accumulator;
  Accumulator() = default;
  Accumulator(string const &id) : Operator<T, V>(id) {
    this->accumulator = 0;
    this->addInput("i1", Accumulator::size);
    this->addOutput("o1");
  }
  string typeName() const override { return "Accumulator"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getLastMessage(inputPort);
    accumulator = accumulator + out.value;
    out.value = accumulator;
    return this->emit(out);
  }
};

}  // namespace rtbot

#endif  // ACCUMULATOR_H
