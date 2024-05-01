#ifndef GREATERTHANSTREAM_H
#define GREATERTHANSTREAM_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct GreaterThanStream : public BinaryJoin<T, V> {
  GreaterThanStream() = default;
  GreaterThanStream(string const &id)
      : BinaryJoin<T, V>(id, [](V a, V b) -> optional<V> {
          if (a > b) return a;
          return nullopt;
        }) {}

  string typeName() const override { return "GreaterThanStream"; }
};

}  // namespace rtbot

#endif  // GREATERTHANSTREAM_H