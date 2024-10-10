#ifndef MESSAGE_H
#define MESSAGE_H

#include <map>
#include <vector>

namespace rtbot {

using Bytes = std::vector<unsigned char>;

template <class T, class V>
struct Message {
  T time;
  V value;

  Message() = default;
  Message(T time, V value) {
    this->time = time;
    this->value = value;
  }

  std::string debug() const { return "(" + std::to_string(time) + ": " + std::to_string(value) + ")"; }

  Bytes collect() const {
    Bytes bytes;

    // Serialize time
    bytes.insert(bytes.end(), reinterpret_cast<const char*>(&time),
                 reinterpret_cast<const char*>(&time) + sizeof(time));

    // Serialize value
    bytes.insert(bytes.end(), reinterpret_cast<const char*>(&value),
                 reinterpret_cast<const char*>(&value) + sizeof(value));

    return bytes;
  }

  void restore(Bytes::const_iterator& it) {
    // Deserialize time
    time = *reinterpret_cast<const T*>(&(*it));
    it += sizeof(time);

    // Deserialize value
    value = *reinterpret_cast<const V*>(&(*it));
    it += sizeof(value);
  }
};

template <class T, class V>
bool operator==(Message<T, V> const& a, Message<T, V> const& b) {
  return a.time == b.time && a.value == b.value;
}

template <class T, class V>
using PortMessage = std::vector<Message<T, V>>;

template <class T, class V>
using OperatorMessage = std::map<std::string, PortMessage<T, V>>;

template <class T, class V>
std::string debug(OperatorMessage<T, V> const& messagesMap) {
  std::string s = "{";
  for (const auto& [port, messages] : messagesMap) {
    s += port + ": [";
    for (const auto& message : messages) {
      s += message.debug() + ", ";
    }
    // remove last comma
    s.pop_back();
    s += "], ";
  }
  s += "}";
  return s;
}

template <class T, class V>
using ProgramMessage = std::map<std::string, OperatorMessage<T, V>>;

template <class T, class V>
std::string debug(ProgramMessage<T, V> const& messagesMap) {
  std::string s = "{";
  for (const auto& [opId, messages] : messagesMap) {
    s += opId + ": " + debug(messages) + ", ";
  }
  // remove last comma
  s.pop_back();

  s += "}";
  return s;
}

}  // namespace rtbot

#endif  // MESSAGE_H
