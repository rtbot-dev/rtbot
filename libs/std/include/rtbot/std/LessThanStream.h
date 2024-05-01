#ifndef LESSTHANSTREAM_H
#define LESSTHANSTREAM_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct LessThanStream : public BinaryJoin<T, V> {
  LessThanStream() = default;
  LessThanStream(string const &id)
      : BinaryJoin<T, V>(id, [](V a, V b) -> optional<V> {
          if (a < b) return a;
          return nullopt;
        }) {}

  string typeName() const override { return "LessThanStream"; }
};

}  // namespace rtbot

#endif  // LESSTHANSTREAM_H