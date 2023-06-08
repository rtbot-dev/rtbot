#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include <cstdint>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Accumulator : public Operator<T, V> {
  V partialSum;
  Accumulator() = default;
  Accumulator(string const &id_) : Operator<T, V>(id_), partialSum(0) {
    this->addInput("i1", 1);
    this->addOutput("o1");
  }
  string typeName() const override { return "Accumulator"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getLastMessage(inputPort);
    partialSum = partialSum + out.value;
    out.value = partialSum;
    return this->emit(out);
  }
};

}  // namespace rtbot

#endif  // ACCUMULATOR_H
