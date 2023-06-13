#ifndef DIFFERENCE_H
#define DIFFERENCE_H

#include <vector>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Difference : public Operator<T, V> {
  Difference() = default;

  Difference(string const &id) : Operator<T, V>(id) {
    this->addInput("i1", Difference<T, V>::size);
    this->addOutput("o1");
  }

  string typeName() const override { return "Difference"; }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> m1 = this->getMessage(inputPort, 1);
    Message<T, V> m0 = this->getMessage(inputPort, 0);
    Message<T, V> out;
    out.value = m0.value - m1.value;
    out.time = m1.time;
    return this->emit(out);
  }

 private:
  static const int size = 2;
};

}  // namespace rtbot

#endif  // DIFFERENCE_H
