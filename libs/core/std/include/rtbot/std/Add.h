#ifndef ADD_H
#define ADD_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Add : public Operator<T, V> {
  Add() = default;
  Add(string const &id, V addend) : Operator<T, V>(id) {
    this->addDataInput("i1", Add::size);
    this->addOutput("o1");
    this->addend = addend;
  }
  string typeName() const override { return "Add"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    out.value = out.value + this->addend;
    return this->emit(out);
  }

  V getAddend() const { return this->addend; }

 private:
  static const size_t size = 1;
  V addend;
};

}  // namespace rtbot

#endif  // ADD_H
