#ifndef COUNT_H
#define COUNT_H

#include <cstdint>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Count : public Operator<T, V> {
  Count() = default;
  Count(string const &id_) : Operator<T, V>(id_, [c = 0](const V &) mutable { return V(++c); }) {}
  string typeName() const override { return "Count"; }
};

}  // namespace rtbot

#endif  // COUNT_H
