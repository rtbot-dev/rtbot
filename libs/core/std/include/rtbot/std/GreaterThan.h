#ifndef GREATERTHAN_H
#define GREATERTHAN_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct GreaterThan : public FilterByValue<T, V> {
  GreaterThan() = default;
  V x;
  GreaterThan(string const &id, V x0) : x(x0), FilterByValue<T, V>(id, [=](V x) { return x > x0; }) {}
  string typeName() const override { return "GreaterThan"; }
};

}  // namespace rtbot

#endif  // GREATERTHAN_H