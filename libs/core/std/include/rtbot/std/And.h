#ifndef AND_H
#define AND_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct And : public BinaryJoin<T, V> {
  And() = default;
  And(string const &id)
      : BinaryJoin<T, V>(id, [](V a, V b) {
          bool left = (a < 0.5) ? false : true;
          bool right = (b >= 0.5) ? true : false;
          bool result = left && right;
          return (result) ? 1 : 0;
        }) {}

  string typeName() const override { return "And"; }
};

}  // namespace rtbot

#endif  // AND_H