#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <vector>

namespace rtbot {

template <class V = double>
struct Message {
  std::uint64_t time;
  V value;

  Message() = default;
  Message(std::uint64_t time_, V value_) : time(time_), value(value_) {}
};

template <class V>
bool operator==(Message<V> const& a, Message<V> const& b) {
  return a.time == b.time && a.value == b.value;
}

}  // namespace rtbot

#endif  // MESSAGE_H
