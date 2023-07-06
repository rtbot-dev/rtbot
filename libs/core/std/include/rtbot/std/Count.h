#ifndef COUNT_H
#define COUNT_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Count : public Operator<T, V> {
  size_t count;
  Count() = default;
  Count(string const &id) : Operator<T, V>(id) {
    this->count = 0;
    this->addDataInput("i1", 1);
    this->addOutput("o1");
  }
  string typeName() const override { return "Count"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    this->count = this->count + 1;
    out.value = this->count;
    return this->emit(out);
  }
};

}  // namespace rtbot

#endif  // COUNT_H
