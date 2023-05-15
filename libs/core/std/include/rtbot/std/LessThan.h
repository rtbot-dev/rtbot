#ifndef LESSTHAN_H
#define LESSTHAN_H

#include <cstdint>

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct LessThan : public FilterByValue<T, V> {
  LessThan() = default;
  V x0;
  LessThan(string const &id_, V x0_) : x0(x0_), FilterByValue<T, V>(id_, [=](V x) { return x < x0_; }) {}
  string typeName() const override { return "LessThan"; }
};

}  // namespace rtbot

#endif  // LESSTHAN_H
