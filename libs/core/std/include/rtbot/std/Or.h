#ifndef OR_H
#define OR_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Or : public BinaryJoin<T, V> {
  Or() = default;
  Or(string const &id)
      : BinaryJoin<T, V>(id, [](V a, V b) {
          bool left = (a < 0.5) ? false : true;
          bool right = (b >= 0.5) ? true : false;
          bool result = left || right;
          return (result) ? 1 : 0;
        }) {}

  string typeName() const override { return "And"; }
};

}  // namespace rtbot

#endif  // OR_H