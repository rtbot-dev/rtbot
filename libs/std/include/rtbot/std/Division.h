#ifndef DIVISION_H
#define DIVISION_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Division : public BinaryJoin<T, V> {
  Division() = default;
  Division(string const &id)
      : BinaryJoin<T, V>(id, [](V a, V b) -> optional<V> {
          if (b != 0) return a / b;
          return nullopt;
        }) {}

  string typeName() const override { return "Division"; }
};

}  // namespace rtbot

#endif  // DIVIDE_H