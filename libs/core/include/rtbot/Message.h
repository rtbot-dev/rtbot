#ifndef MESSAGE_H
#define MESSAGE_H

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

}  // namespace rtbot

#endif  // MESSAGE_H
