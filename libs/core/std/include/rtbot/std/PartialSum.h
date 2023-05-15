#ifndef PARTIALSUM_H
#define PARTIALSUM_H

#include <cstdint>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct PartialSum : public Operator<T, V> {
  PartialSum() = default;
  PartialSum(string const &id_) : Operator<T, V>(id_, [s = V(0)](const V &x) mutable { return s += x; }) {}
  string typeName() const override { return "PartialSum"; }
};

}  // namespace rtbot

#endif  // PARTIALSUM_H
