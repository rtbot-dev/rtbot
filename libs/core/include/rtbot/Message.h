#ifndef MESSAGE_H
#define MESSAGE_H

#include <map>
#include <vector>

namespace rtbot {

template <class T, class V>
struct Message {
  T time;
  V value;

  Message() = default;
  Message(T time, V value) {
    this->time = time;
    this->value = value;
  }
};

template <class T, class V>
bool operator==(Message<T, V> const& a, Message<T, V> const& b) {
  return a.time == b.time && a.value == b.value;
}

template <class T, class V>
using Messages = std::vector<Message<T, V>>;

template <class T, class V>
using PortPayload = std::map<std::string, Messages<T, V>>;

template <class T, class V>
using OperatorPayload = std::map<std::string, PortPayload<T, V>>;

}  // namespace rtbot

#endif  // MESSAGE_H
