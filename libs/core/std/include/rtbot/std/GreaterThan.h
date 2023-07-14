#ifndef GREATERTHAN_H
#define GREATERTHAN_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct GreaterThan : public FilterByValue<T, V> {
  GreaterThan() = default;
  V x;
  GreaterThan(string const &id, V value)
      : x(value), FilterByValue<T, V>(id, [=](V number) { return number > value; }) {}
  string typeName() const override { return "GreaterThan"; }
};

}  // namespace rtbot

#endif  // GREATERTHAN_H