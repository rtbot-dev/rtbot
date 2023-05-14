#ifndef FILTERBYVALUE_H
#define FILTERBYVALUE_H

#include <functional>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct FilterByValue : public Operator<T, V> {
  std::function<bool(V)> filter;

  FilterByValue()=default;
  FilterByValue(string const& id_, std::function<bool(V)> filter_) : Operator<T, V>(id_), filter(filter_) {}

  map<string, std::vector<Message<T, V>>> receive(Message<T, V> const& msg) override {
    if (filter(msg.value)) return this->emit(msg);
    return {};
  }
};
}  // namespace rtbot

#endif  // FILTERBYVALUE_H
