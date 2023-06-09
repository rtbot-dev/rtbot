#ifndef EQUALTO_H
#define EQUALTO_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct EqualTo : public FilterByValue<T, V> {
  EqualTo() = default;
  V x0;
  EqualTo(string const &id_, V x0_) : x0(x0_), FilterByValue<T, V>(id_, [=](V x) { return x == x0_; }) {}
  string typeName() const override { return "EqualTo"; }
};

}  // namespace rtbot

#endif  // EQUALTO_H