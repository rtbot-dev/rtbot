#ifndef EQUALTO_H
#define EQUALTO_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct EqualTo : public FilterByValue<T, V> {
  EqualTo() = default;
  V x;
  EqualTo(string const &id, V x0) : x(x0), FilterByValue<T, V>(id, [=](V x) { return x == x0; }) {}
  string typeName() const override { return "EqualTo"; }
};

}  // namespace rtbot

#endif  // EQUALTO_H