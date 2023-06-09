#ifndef COUNT_H
#define COUNT_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Count : public Operator<T, V> {
  size_t count;
  Count() = default;
  Count(string const &id_) : Operator<T, V>(id_), count(0) {
    this->addInput("i1", 1);
    this->addOutput("o1");
  }
  string typeName() const override { return "Count"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getLastMessage(inputPort);
    count = count + 1;
    out.value = count;
    return this->emit(out);
  }
};

}  // namespace rtbot

#endif  // COUNT_H
