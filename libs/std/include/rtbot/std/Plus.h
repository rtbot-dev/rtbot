#ifndef PLUS_H
#define PLUS_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Plus : public BinaryJoin<T, V> {
  Plus() = default;
  Plus(string const &id) : BinaryJoin<T, V>(id, [](V a, V b) -> optional<V> { return a + b; }) {}

  string typeName() const override { return "Plus"; }
};

}  // namespace rtbot

#endif  // PLUS_H
