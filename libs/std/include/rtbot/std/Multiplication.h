#ifndef MULTIPLICATION_H
#define MULTIPLICATION_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Multiplication : public BinaryJoin<T, V> {
  Multiplication() = default;
  Multiplication(string const &id) : BinaryJoin<T, V>(id, [](V a, V b) -> optional<V> { return a * b; }) {}

  string typeName() const override { return "Multiplication"; }
};

}  // namespace rtbot

#endif  // MULTIPLICATION_H