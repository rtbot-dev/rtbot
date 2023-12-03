#ifndef MINUS_H
#define MINUS_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Minus : public BinaryJoin<T, V> {
  Minus() = default;
  Minus(string const &id) : BinaryJoin<T, V>(id, [](V a, V b) { return a - b; }) {}

  string typeName() const override { return "Minus"; }
};

}  // namespace rtbot

#endif  // MINUS_H