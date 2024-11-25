#ifndef MESSAGE_H
#define MESSAGE_H

#include <typeindex>
#include <vector>

namespace rtbot {
using Bytes = std::vector<unsigned char>;

using timestamp_t = std::int64_t;

// Core data types
struct NumberData {
  double value;
};
struct BooleanData {
  bool value;
};
struct VectorNumberData {
  std::vector<double> values;
};
struct VectorBooleanData {
  std::vector<bool> values;
};

// Base message struct
struct BaseMessage {
  timestamp_t time;
  virtual ~BaseMessage() = default;
  virtual std::unique_ptr<BaseMessage> clone() const = 0;
  virtual std::type_index type() const = 0;
};

// Type-specific message implementation
template <typename T>
struct Message : BaseMessage {
  T data;

  std::unique_ptr<BaseMessage> clone() const override {
    auto cloned = std::make_unique<Message<T>>();
    cloned->time = this->time;
    cloned->data = this->data;
    return cloned;
  }

  std::type_index type() const override { return std::type_index(typeid(T)); }
};

// Helper to create typed message
template <typename T>
std::unique_ptr<Message<T>> create_message(timestamp_t time, T data) {
  auto msg = std::make_unique<Message<T>>();
  msg->time = time;
  msg->data = std::move(data);
  return msg;
}

}  // namespace rtbot

#endif  // MESSAGE_H