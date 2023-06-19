#ifndef FILTERBYVALUE_H
#define FILTERBYVALUE_H

#include <functional>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct FilterByValue : public Operator<T, V> {
  std::function<bool(V)> filter;

  FilterByValue() = default;
  FilterByValue(string const& id, std::function<bool(V)> filter) : Operator<T, V>(id) {
    this->filter = filter;
    this->addDataInput("i1", 1);
    this->addOutput("o1");
  }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    if (filter(out.value)) return this->emit(out);
    return {};
  }
};
}  // namespace rtbot

#endif  // FILTERBYVALUE_H
