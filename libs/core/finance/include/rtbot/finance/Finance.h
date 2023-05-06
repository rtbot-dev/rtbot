#ifndef FINANCE_H
#define FINANCE_H

#include <cstdint>
#include "rtbot/FilterByValue.h"

namespace rtbot {

namespace tools {

template <class T, class V>
struct Count : public Operator<T,V> {
  Count(string const &id_) : Operator<T,V>(id_, [c = 0](const V &) mutable { return V(++c); }) {}
};

template <class T, class V>
struct PartialSum : public Operator<T,V> {
  PartialSum(string const &id_) : Operator<T,V>(id_, [s = V(0)](const V &x) mutable { return s += x; }) {}
};

template <class T, class V>
struct FilterGreaterThan : public FilterByValue<T,V> {
  FilterGreaterThan(string const &id_, double x0 = 0) : FilterByValue<T,V>(id_, [=](double x) { return x > x0; }) {}
};

template <class T, class V>
struct FilterLessThan : public FilterByValue<T,V> {
  FilterLessThan(string const &id_, double x0 = 0) : FilterByValue<T,V>(id_, [=](double x) { return x < x0; }) {}
};

}  // namespace tools

}  // namespace rtbot

#endif  // FINANCE_H
