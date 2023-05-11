#ifndef GREATERTHAN_H
#define GREATERTHAN_H

#include <cstdint>

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct GreaterThan : public FilterByValue<T, V> {
  GreaterThan() = default;
  V x0;
  GreaterThan(string const &id_, V x0_) : x0(x0_), FilterByValue<T, V>(id_, [=](V x) { return x > x0_; }) {}
  string typeName() const override { return "GreaterThan"; }
};

}  // namespace rtbot

#endif  // FINANCE_H