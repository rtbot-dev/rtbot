#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <vector>

namespace rtbot {

template <class T, class V>
struct Message {
  T time;
  V value;

  Message() = default;
  Message(T time_, V value_) : time(time_), value(value_) {}
};

template <class T, class V>
bool operator==(Message<T, V> const& a, Message<T, V> const& b) {
  return a.time == b.time && a.value == b.value;
}

}  // namespace rtbot

#endif  // MESSAGE_H
