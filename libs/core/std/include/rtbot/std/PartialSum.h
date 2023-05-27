#ifndef PARTIALSUM_H
#define PARTIALSUM_H

#include <cstdint>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct PartialSum : public Operator<T, V> {
  V partialSum;
  PartialSum() = default;
  PartialSum(string const &id_) : Operator<T, V>(id_), partialSum(0) {
    this->addInput("i1", 1);
    this->addOutput("o1");
  }
  string typeName() const override { return "PartialSum"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getLastMessage(inputPort);
    partialSum = partialSum + out.value;
    out.value = partialSum;
    return this->emit(out);
  }
};

}  // namespace rtbot

#endif  // PARTIALSUM_H
