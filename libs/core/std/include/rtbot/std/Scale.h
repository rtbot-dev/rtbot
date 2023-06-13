#ifndef SCALE_H
#define SCALE_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Scale : public Operator<T, V> {
  Scale() = default;
  Scale(string const &id, V factor) : Operator<T, V>(id) {
    this->addInput("i1", Scale::size);
    this->addOutput("o1");
    this->factor = factor;
  }
  string typeName() const override { return "Scale"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getLastMessage(inputPort);
    out.value = out.value * this->factor;
    return this->emit(out);
  }

  V getFactor() const { return this->factor; }

 private:
  static const size_t size = 1;
  V factor;
};

}  // namespace rtbot

#endif  // SCALE_H
