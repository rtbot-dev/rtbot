#ifndef POWER_H
#define POWER_H

#include <cmath>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Power : public Operator<T, V> {
  Power() = default;
  Power(string const &id, V power) : Operator<T, V>(id) {
    this->addDataInput("i1", Power::size);
    this->addOutput("o1");
    this->power = power;
  }
  string typeName() const override { return "Power"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    out.value = pow(out.value, this->power);
    return this->emit(out);
  }

  V getPower() const { return this->power; }

 private:
  static const size_t size = 1;
  V power;
};

}  // namespace rtbot

#endif  // POWER_H
