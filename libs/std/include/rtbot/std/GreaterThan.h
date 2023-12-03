#ifndef GREATERTHAN_H
#define GREATERTHAN_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct GreaterThan : public FilterByValue<T, V> {
  GreaterThan() = default;

  GreaterThan(string const &id, V value) : FilterByValue<T, V>(id, [=](V number) { return number > value; }) {
    this->value = value;
  }
  string typeName() const override { return "GreaterThan"; }

  V getValue() const { return this->value; }

 private:
  V value;
};

}  // namespace rtbot

#endif  // GREATERTHAN_H
