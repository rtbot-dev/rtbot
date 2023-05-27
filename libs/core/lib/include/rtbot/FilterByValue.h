#ifndef FILTERBYVALUE_H
#define FILTERBYVALUE_H

#include <functional>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct FilterByValue : public Operator<T, V> {
  std::function<bool(V)> filter;

  FilterByValue() = default;
  FilterByValue(string const& id_, std::function<bool(V)> filter_) : Operator<T, V>(id_), filter(filter_) {
    this->addInput("i1", 1);
    this->addOutput("o1");
  }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getLastMessage(inputPort);
    if (filter(out.value)) return this->emit(out);
    return {};
  }
};
}  // namespace rtbot

#endif  // FILTERBYVALUE_H
