#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <vector>

namespace rtbot {

using timestamp_t = int64_t;
using Bytes = std::vector<uint8_t>;

// Forward declarations
template <typename T>
class Message;

struct NumberData;
struct BooleanData;
struct VectorNumberData;
struct VectorBooleanData;

// Data type definitions
struct NumberData {
  double value;

  Bytes serialize() const {
    Bytes bytes;
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&value),
                 reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
    return bytes;
  }

  static NumberData deserialize(Bytes::const_iterator& it) {
    NumberData data;
    data.value = *reinterpret_cast<const double*>(&(*it));
    it += sizeof(double);
    return data;
  }
};

struct BooleanData {
  bool value;

  Bytes serialize() const {
    Bytes bytes;
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&value),
                 reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
    return bytes;
  }

  static BooleanData deserialize(Bytes::const_iterator& it) {
    BooleanData data;
    data.value = *reinterpret_cast<const bool*>(&(*it));
    it += sizeof(bool);
    return data;
  }
};

struct VectorNumberData {
  std::vector<double> values;

  Bytes serialize() const {
    Bytes bytes;
    size_t size = values.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&size),
                 reinterpret_cast<const uint8_t*>(&size) + sizeof(size));

    for (const auto& value : values) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&value),
                   reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
    }
    return bytes;
  }

  static VectorNumberData deserialize(Bytes::const_iterator& it) {
    VectorNumberData data;
    size_t size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    data.values.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      double value = *reinterpret_cast<const double*>(&(*it));
      it += sizeof(double);
      data.values.push_back(value);
    }
    return data;
  }
};

struct VectorBooleanData {
  std::vector<bool> values;

  Bytes serialize() const {
    Bytes bytes;
    size_t size = values.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&size),
                 reinterpret_cast<const uint8_t*>(&size) + sizeof(size));

    // Pack boolean values into bytes
    size_t byte_count = (size + 7) / 8;
    std::vector<uint8_t> packed_bools(byte_count, 0);

    for (size_t i = 0; i < size; ++i) {
      if (values[i]) {
        packed_bools[i / 8] |= (1 << (i % 8));
      }
    }

    bytes.insert(bytes.end(), packed_bools.begin(), packed_bools.end());
    return bytes;
  }

  static VectorBooleanData deserialize(Bytes::const_iterator& it) {
    VectorBooleanData data;
    size_t size = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    // Read packed boolean values
    size_t byte_count = (size + 7) / 8;
    data.values.reserve(size);

    for (size_t i = 0; i < size; ++i) {
      uint8_t byte = *(it + (i / 8));
      bool value = (byte & (1 << (i % 8))) != 0;
      data.values.push_back(value);
    }

    it += byte_count;
    return data;
  }
};

// Base message class
class BaseMessage {
 public:
  BaseMessage(timestamp_t t) : time(t) {}
  virtual ~BaseMessage() = default;

  timestamp_t time;
  virtual std::type_index type() const = 0;
  virtual std::unique_ptr<BaseMessage> clone() const = 0;
  virtual std::string to_string() const = 0;

  virtual Bytes serialize() const {
    Bytes bytes;

    // Serialize timestamp
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                 reinterpret_cast<const uint8_t*>(&time) + sizeof(time));

    // Serialize type information
    std::string type_name = type().name();
    size_t type_length = type_name.length();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&type_length),
                 reinterpret_cast<const uint8_t*>(&type_length) + sizeof(type_length));
    bytes.insert(bytes.end(), type_name.begin(), type_name.end());

    return bytes;
  }

  // Forward declare deserialization methods that will be defined after Message template
  static std::unique_ptr<BaseMessage> deserialize(const Bytes& bytes);
  template <typename T>
  static std::unique_ptr<Message<T>> deserialize_as(const Bytes& bytes);
};

// Message template class
template <typename T>
class Message : public BaseMessage {
 public:
  Message(timestamp_t t, const T& d) : BaseMessage(t), data(d) {}

  std::type_index type() const override { return typeid(T); }

  std::unique_ptr<BaseMessage> clone() const override { return std::make_unique<Message<T>>(time, data); }

  std::string to_string() const override {
    std::ostringstream ss;
    ss << "(" << time << ", ";

    if constexpr (std::is_same_v<T, NumberData>) {
      ss << data.value;
    } else if constexpr (std::is_same_v<T, BooleanData>) {
      ss << (data.value ? "true" : "false");
    } else if constexpr (std::is_same_v<T, VectorNumberData>) {
      ss << "[";
      for (size_t i = 0; i < data.values.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << data.values[i];
      }
      ss << "]";
    } else if constexpr (std::is_same_v<T, VectorBooleanData>) {
      ss << "[";
      for (size_t i = 0; i < data.values.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << (data.values[i] ? "true" : "false");
      }
      ss << "]";
    }

    ss << ")";
    return ss.str();
  }

  Bytes serialize() const override {
    // First serialize base message data
    Bytes bytes = BaseMessage::serialize();

    // Then serialize the specific data
    Bytes data_bytes = data.serialize();
    bytes.insert(bytes.end(), data_bytes.begin(), data_bytes.end());

    return bytes;
  }

  static std::unique_ptr<Message<T>> deserialize_data(timestamp_t time, Bytes::const_iterator& it,
                                                      Bytes::const_iterator end) {
    T data = T::deserialize(it);
    return std::make_unique<Message<T>>(time, data);
  }

  T data;
};

// Define BaseMessage's deserialization methods after Message template is complete
inline std::unique_ptr<BaseMessage> BaseMessage::deserialize(const Bytes& bytes) {
  auto it = bytes.begin();

  // Read timestamp
  timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
  it += sizeof(timestamp_t);

  // Read type information
  size_t type_length = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);
  std::string type_name(it, it + type_length);
  it += type_length;

  // Create appropriate message type based on type_name
  if (type_name == typeid(NumberData).name()) {
    return Message<NumberData>::deserialize_data(time, it, bytes.end());
  } else if (type_name == typeid(BooleanData).name()) {
    return Message<BooleanData>::deserialize_data(time, it, bytes.end());
  } else if (type_name == typeid(VectorNumberData).name()) {
    return Message<VectorNumberData>::deserialize_data(time, it, bytes.end());
  } else if (type_name == typeid(VectorBooleanData).name()) {
    return Message<VectorBooleanData>::deserialize_data(time, it, bytes.end());
  } else {
    throw std::runtime_error("Unknown message type: " + type_name);
  }
}

template <typename T>
inline std::unique_ptr<Message<T>> BaseMessage::deserialize_as(const Bytes& bytes) {
  auto base_msg = deserialize(bytes);
  if (base_msg->type() != typeid(T)) {
    throw std::runtime_error("Type mismatch in deserialize_as");
  }
  auto* typed_msg = dynamic_cast<Message<T>*>(base_msg.get());
  if (!typed_msg) {
    throw std::runtime_error("Failed to cast message to requested type");
  }
  return std::unique_ptr<Message<T>>(static_cast<Message<T>*>(base_msg.release()));
}

// Helper functions
template <typename T>
std::unique_ptr<Message<T>> create_message(timestamp_t time, const T& data) {
  return std::make_unique<Message<T>>(time, data);
}

}  // namespace rtbot

#endif  // MESSAGE_H