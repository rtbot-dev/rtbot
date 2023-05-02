#ifndef FILTERBYVALUE_H
#define FILTERBYVALUE_H

#include <functional>

#include "rtbot/Operator.h"

namespace rtbot {
template <class T>
struct FilterByValue : public Operator<T> {
  std::function<bool(T)> filter;

  FilterByValue(string const& id_, std::function<bool(T)> filter_) : Operator<T>(id_), filter(filter_) {}

  map<string, Message<T>> receive(Message<T> const& msg) override {
    if (all_of(msg.value.begin(), msg.value.end(), filter)) return this->emit(msg);
    return {};
  }
};
}  // namespace rtbot

#endif  // FILTERBYVALUE_H
