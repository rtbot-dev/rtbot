#ifndef FINANCE_H
#define FINANCE_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

namespace tools {

template <class V = double>
struct Count : public Operator<V> {
  Count(string const &id_) : Operator<V>(id_, [c = 0](const V &) mutable { return V(++c); }) {}
};

template <class V = double>
struct PartialSum : public Operator<V> {
  PartialSum(string const &id_) : Operator<V>(id_, [s = V(0)](const V &x) mutable { return s += x; }) {}
};

template <class V = double>
struct FilterGreaterThan : public FilterByValue<V> {
  FilterGreaterThan(string const &id_, double x0 = 0) : FilterByValue<V>(id_, [=](double x) { return x > x0; }) {}
};

template <class V = double>
struct FilterLessThan : public FilterByValue<V> {
  FilterLessThan(string const &id_, double x0 = 0) : FilterByValue<V>(id_, [=](double x) { return x < x0; }) {}
};

}  // namespace tools

}  // namespace rtbot

#endif  // FINANCE_H
