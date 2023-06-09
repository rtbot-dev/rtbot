#ifndef LESSTHAN_H
#define LESSTHAN_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct LessThan : public FilterByValue<T, V> {
  LessThan() = default;
  V x;
  LessThan(string const &id, V x0) : x(x0), FilterByValue<T, V>(id, [=](V x) { return x < x0; }) {}
  string typeName() const override { return "LessThan"; }
};

}  // namespace rtbot

#endif  // LESSTHAN_H
