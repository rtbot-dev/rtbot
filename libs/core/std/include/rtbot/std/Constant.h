#ifndef CONST_H
#define CONST_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Constant : public Operator<T, V> {
  Constant() = default;
  Constant(string const &id, V constant) : Operator<T, V>(id) {
    this->addDataInput("i1", Constant::size);
    this->addOutput("o1");
    this->constant = constant;
  }
  string typeName() const override { return "Constant"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    out.value = this->constant;
    return this->emit(out);
  }

  V getConstant() const { return this->constant; }

 private:
  static const size_t size = 1;
  V constant;
};

}  // namespace rtbot

#endif  // CONST_H
