#ifndef FILTERBYVALUE_H
#define FILTERBYVALUE_H

#include <functional>

#include "rtbot/Operator.h"

namespace rtbot {

template <class V = double>
struct FilterByValue : public Operator<V> {
  std::function<bool(V)> filter;

  FilterByValue(string const& id_, std::function<bool(V)> filter_) : Operator<V>(id_), filter(filter_) {}

  map<string, Message<V>> receive(Message<V> const& msg) override {
    if (filter(msg.value)) return this->emit(msg);
    return {};
  }
};
}  // namespace rtbot

#endif  // FILTERBYVALUE_H
