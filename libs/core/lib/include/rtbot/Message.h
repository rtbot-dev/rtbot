#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <vector>

namespace rtbot {

template <class T = double>
struct Message {
  std::uint64_t time;
  std::vector<T> value;

  Message() = default;
  Message(std::uint64_t time_, T value_) : time(time_), value(1, value_) {}
  Message(std::uint64_t time_, std::vector<T> const& value_) : time(time_), value(value_) {}
};

template <class T>
bool operator==(Message<T> const& a, Message<T> const& b) {
  return a.time == b.time && a.value == b.value;
}

}  // namespace rtbot

#endif  // MESSAGE_H
