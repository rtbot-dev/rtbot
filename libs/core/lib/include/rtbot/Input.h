#ifndef INPUT_H
#define INPUT_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Input : public Operator<T, V> {
  static const int size = 2;

  Input() = default;

  Input(string const &id) : Operator<T, V>(id) {
    this->addDataInput("i1", Input<T, V>::size);
    this->addOutput("o1");
  }

  string typeName() const override { return "Input"; }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> m1 = this->getDataInputMessage(inputPort, 1);
    Message<T, V> m0 = this->getDataInputMessage(inputPort, 0);
    if (m1.time <= m0.time) return {};
    return this->emit(m0);
  }
};

}  // namespace rtbot

#endif  // INPUT_H
