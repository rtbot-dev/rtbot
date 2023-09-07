#ifndef LESSTHAN_H
#define LESSTHAN_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct LessThan : public FilterByValue<T, V> {
  LessThan() = default;

  LessThan(string const &id, V value) : FilterByValue<T, V>(id, [=](V number) { return number < value; }) {
    this->value = value;
  }
  string typeName() const override { return "LessThan"; }

  V getValue() const { return this->value; }

 private:
  V value;
};

}  // namespace rtbot

#endif  // LESSTHAN_H
