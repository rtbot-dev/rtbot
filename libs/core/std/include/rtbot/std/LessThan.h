#ifndef LESSTHAN_H
#define LESSTHAN_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct LessThan : public FilterByValue<T, V> {
  LessThan() = default;
  V x;
  LessThan(string const &id, V value) : x(value), FilterByValue<T, V>(id, [=](V number) { return number < value; }) {}
  string typeName() const override { return "LessThan"; }
};

}  // namespace rtbot

#endif  // LESSTHAN_H
