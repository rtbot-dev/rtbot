// StateSerializer.h
#ifndef STATE_SERIALIZER_H
#define STATE_SERIALIZER_H

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <typeindex>
#include <vector>

#include "rtbot/Message.h"

namespace rtbot {

using MessageQueue = std::deque<std::unique_ptr<BaseMessage>>;

class StateSerializer {
 public:
  // Core serialization methods
  static uint64_t fnv1a(const std::string& s);

  static uint64_t hash_double(double value);

  static void serialize_checksum(Bytes& bytes, std::uint64_t checksum);
  static std::uint64_t deserialize_checksum(Bytes::const_iterator& it);

  static void serialize_timestamp_set(Bytes& bytes, const std::set<timestamp_t>& times);
  static void deserialize_timestamp_set(Bytes::const_iterator& it, std::set<timestamp_t>& times);

  static void serialize_timestamp_bool_map(Bytes& bytes, const std::map<timestamp_t, bool>& time_map);
  static void deserialize_timestamp_bool_map(Bytes::const_iterator& it, std::map<timestamp_t, bool>& time_map);

  static void serialize_port_map(Bytes& bytes, const std::map<size_t, std::set<timestamp_t>>& port_map);
  static void deserialize_port_map(Bytes::const_iterator& it, std::map<size_t, std::set<timestamp_t>>& port_map);

  static void serialize_control_map(Bytes& bytes, const std::map<size_t, std::map<timestamp_t, bool>>& control_map);
  static void deserialize_control_map(Bytes::const_iterator& it,
                                      std::map<size_t, std::map<timestamp_t, bool>>& control_map);

  static void serialize_string_vector(Bytes& bytes, const std::vector<std::string>& strings);
  static void deserialize_string_vector(Bytes::const_iterator& it, std::vector<std::string>& strings);

  static void serialize_type_index(Bytes& bytes, const std::type_index& type);
  static void validate_and_restore_type(Bytes::const_iterator& it, const std::type_index& expected_type);

  static void serialize_message_queue(Bytes& bytes, const MessageQueue& queue);
  static void deserialize_message_queue(Bytes::const_iterator& it, MessageQueue& queue);

  static void serialize_index_set(Bytes& bytes, const std::set<size_t>& indices);
  static void deserialize_index_set(Bytes::const_iterator& it, std::set<size_t>& indices);

  // Port validation helper
  static void validate_port_count(size_t stored_count, size_t actual_count, const std::string& port_type);

  // Map serialization helpers
  static void serialize_port_control_map(Bytes& bytes,
                                         const std::map<size_t, std::map<timestamp_t, bool>>& control_map);
  static void deserialize_port_control_map(Bytes::const_iterator& it,
                                           std::map<size_t, std::map<timestamp_t, bool>>& control_map);

  // Set serialization helpers
  static void serialize_port_timestamp_set_map(Bytes& bytes, const std::map<size_t, std::set<timestamp_t>>& port_map);
  static void deserialize_port_timestamp_set_map(Bytes::const_iterator& it,
                                                 std::map<size_t, std::set<timestamp_t>>& port_map);

 private:
  // Private constructor to prevent instantiation
  StateSerializer() = default;
};

inline uint64_t StateSerializer::fnv1a(const std::string& s) {
    uint64_t hash = 1469598103934665603ULL;     // FNV offset basis
    for (unsigned char c : s) {
        hash ^= c;
        hash *= 1099511628211ULL;               // FNV prime
    }
    return hash;
}

inline uint64_t StateSerializer::hash_double(double value) {
    uint64_t u;
    uint64_t quantized = static_cast<uint64_t>(value * 1e9);
    std::memcpy(&u, &quantized, sizeof(uint64_t));
    return u;
}

inline void StateSerializer::serialize_checksum(Bytes& bytes, std::uint64_t checksum) {
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&checksum),
               reinterpret_cast<const uint8_t*>(&checksum) + sizeof(checksum));
}

inline std::uint64_t StateSerializer::deserialize_checksum(Bytes::const_iterator& it) {
  std::uint64_t checksum = 0;
  std::memcpy(&checksum, &(*it), sizeof(uint64_t));
  it += sizeof(uint64_t);
  return checksum;
}

inline void StateSerializer::serialize_timestamp_set(Bytes& bytes, const std::set<timestamp_t>& times) {
  size_t size = times.size();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&size),
               reinterpret_cast<const uint8_t*>(&size) + sizeof(size));

  for (const auto& time : times) {
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                 reinterpret_cast<const uint8_t*>(&time) + sizeof(time));
  }
}

inline void StateSerializer::deserialize_timestamp_set(Bytes::const_iterator& it, std::set<timestamp_t>& times) {
  size_t size = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);

  times.clear();
  for (size_t i = 0; i < size; ++i) {
    timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
    it += sizeof(timestamp_t);
    times.insert(time);
  }
}

inline void StateSerializer::serialize_timestamp_bool_map(Bytes& bytes, const std::map<timestamp_t, bool>& time_map) {
  size_t size = time_map.size();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&size),
               reinterpret_cast<const uint8_t*>(&size) + sizeof(size));

  for (const auto& [time, value] : time_map) {
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&time),
                 reinterpret_cast<const uint8_t*>(&time) + sizeof(time));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&value),
                 reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
  }
}

inline void StateSerializer::deserialize_timestamp_bool_map(Bytes::const_iterator& it,
                                                            std::map<timestamp_t, bool>& time_map) {
  size_t size = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);

  time_map.clear();
  for (size_t i = 0; i < size; ++i) {
    timestamp_t time = *reinterpret_cast<const timestamp_t*>(&(*it));
    it += sizeof(timestamp_t);

    bool value = *reinterpret_cast<const bool*>(&(*it));
    it += sizeof(bool);

    time_map[time] = value;
  }
}

inline void StateSerializer::serialize_port_map(Bytes& bytes, const std::map<size_t, std::set<timestamp_t>>& port_map) {
  size_t num_ports = port_map.size();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&num_ports),
               reinterpret_cast<const uint8_t*>(&num_ports) + sizeof(num_ports));

  for (const auto& [port, times] : port_map) {
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port),
                 reinterpret_cast<const uint8_t*>(&port) + sizeof(port));
    serialize_timestamp_set(bytes, times);
  }
}

inline void StateSerializer::deserialize_port_map(Bytes::const_iterator& it,
                                                  std::map<size_t, std::set<timestamp_t>>& port_map) {
  size_t num_ports = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);

  port_map.clear();
  for (size_t i = 0; i < num_ports; ++i) {
    size_t port = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    deserialize_timestamp_set(it, port_map[port]);
  }
}

inline void StateSerializer::serialize_control_map(Bytes& bytes,
                                                   const std::map<size_t, std::map<timestamp_t, bool>>& control_map) {
  size_t num_ports = control_map.size();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&num_ports),
               reinterpret_cast<const uint8_t*>(&num_ports) + sizeof(num_ports));

  for (const auto& [port, times] : control_map) {
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port),
                 reinterpret_cast<const uint8_t*>(&port) + sizeof(port));
    serialize_timestamp_bool_map(bytes, times);
  }
}

inline void StateSerializer::deserialize_control_map(Bytes::const_iterator& it,
                                                     std::map<size_t, std::map<timestamp_t, bool>>& control_map) {
  size_t num_ports = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);

  control_map.clear();
  for (size_t i = 0; i < num_ports; ++i) {
    size_t port = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    deserialize_timestamp_bool_map(it, control_map[port]);
  }
}

inline void StateSerializer::serialize_string_vector(Bytes& bytes, const std::vector<std::string>& strings) {
  size_t size = strings.size();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&size),
               reinterpret_cast<const uint8_t*>(&size) + sizeof(size));

  for (const auto& str : strings) {
    size_t str_length = str.length();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&str_length),
                 reinterpret_cast<const uint8_t*>(&str_length) + sizeof(str_length));
    bytes.insert(bytes.end(), str.begin(), str.end());
  }
}

inline void StateSerializer::deserialize_string_vector(Bytes::const_iterator& it, std::vector<std::string>& strings) {
  size_t size = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);

  strings.clear();
  strings.reserve(size);

  for (size_t i = 0; i < size; ++i) {
    size_t str_length = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    strings.push_back(std::string(it, it + str_length));
    it += str_length;
  }
}

inline void StateSerializer::serialize_type_index(Bytes& bytes, const std::type_index& type) {
  std::string type_name = type.name();
  size_t name_length = type_name.length();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&name_length),
               reinterpret_cast<const uint8_t*>(&name_length) + sizeof(name_length));
  bytes.insert(bytes.end(), type_name.begin(), type_name.end());
}

inline void StateSerializer::validate_and_restore_type(Bytes::const_iterator& it,
                                                       const std::type_index& expected_type) {
  size_t name_length = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);
  std::string stored_type_name(it, it + name_length);
  it += name_length;

  if (stored_type_name != expected_type.name()) {
    throw std::runtime_error("Port type mismatch during restore: stored=" + stored_type_name +
                             ", expected=" + expected_type.name());
  }
}

inline void StateSerializer::serialize_message_queue(Bytes& bytes, const MessageQueue& queue) {
  size_t queue_size = queue.size();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&queue_size),
               reinterpret_cast<const uint8_t*>(&queue_size) + sizeof(queue_size));

  for (const auto& msg : queue) {
    Bytes msg_bytes = msg->serialize();
    size_t msg_size = msg_bytes.size();

    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&msg_size),
                 reinterpret_cast<const uint8_t*>(&msg_size) + sizeof(msg_size));
    bytes.insert(bytes.end(), msg_bytes.begin(), msg_bytes.end());
  }
}

inline void StateSerializer::deserialize_message_queue(Bytes::const_iterator& it, MessageQueue& queue) {
  queue.clear();

  // ---- read queue_size ----
  size_t queue_size;
  std::memcpy(&queue_size, &(*it), sizeof(queue_size));
  it += sizeof(queue_size);

  // ---- read each message ----
  for (size_t i = 0; i < queue_size; ++i) {
      // read the size of the message
      size_t msg_size;
      std::memcpy(&msg_size, &(*it), sizeof(msg_size));
      it += sizeof(msg_size);

      // extract message bytes
      Bytes msg_bytes(it, it + msg_size);

      // deserialize the message
      queue.push_back(BaseMessage::deserialize(msg_bytes));

      // advance iterator
      it += msg_size;
  }
}

inline void StateSerializer::serialize_index_set(Bytes& bytes, const std::set<size_t>& indices) {
  size_t set_size = indices.size();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&set_size),
               reinterpret_cast<const uint8_t*>(&set_size) + sizeof(set_size));

  for (const auto& index : indices) {
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&index),
                 reinterpret_cast<const uint8_t*>(&index) + sizeof(index));
  }
}

inline void StateSerializer::deserialize_index_set(Bytes::const_iterator& it, std::set<size_t>& indices) {
  indices.clear();
  size_t set_size = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);

  for (size_t i = 0; i < set_size; ++i) {
    size_t index = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);
    indices.insert(index);
  }
}

inline void StateSerializer::validate_port_count(size_t stored_count, size_t actual_count,
                                                 const std::string& port_type) {
  if (stored_count != actual_count) {
    throw std::runtime_error(port_type + " port count mismatch in restore: stored=" + std::to_string(stored_count) +
                             ", actual=" + std::to_string(actual_count));
  }
}

inline void StateSerializer::serialize_port_timestamp_set_map(Bytes& bytes,
                                                              const std::map<size_t, std::set<timestamp_t>>& port_map) {
  size_t num_ports = port_map.size();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&num_ports),
               reinterpret_cast<const uint8_t*>(&num_ports) + sizeof(num_ports));

  for (const auto& [port, times] : port_map) {
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port),
                 reinterpret_cast<const uint8_t*>(&port) + sizeof(port));
    serialize_timestamp_set(bytes, times);
  }
}

inline void StateSerializer::deserialize_port_timestamp_set_map(Bytes::const_iterator& it,
                                                                std::map<size_t, std::set<timestamp_t>>& port_map) {
  size_t num_ports = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);

  port_map.clear();
  for (size_t i = 0; i < num_ports; ++i) {
    size_t port = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    deserialize_timestamp_set(it, port_map[port]);
  }
}

inline void StateSerializer::serialize_port_control_map(
    Bytes& bytes, const std::map<size_t, std::map<timestamp_t, bool>>& control_map) {
  size_t num_ports = control_map.size();
  bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&num_ports),
               reinterpret_cast<const uint8_t*>(&num_ports) + sizeof(num_ports));

  for (const auto& [port, times] : control_map) {
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port),
                 reinterpret_cast<const uint8_t*>(&port) + sizeof(port));
    serialize_timestamp_bool_map(bytes, times);
  }
}

inline void StateSerializer::deserialize_port_control_map(Bytes::const_iterator& it,
                                                          std::map<size_t, std::map<timestamp_t, bool>>& control_map) {
  size_t num_ports = *reinterpret_cast<const size_t*>(&(*it));
  it += sizeof(size_t);

  control_map.clear();
  for (size_t i = 0; i < num_ports; ++i) {
    size_t port = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(size_t);

    deserialize_timestamp_bool_map(it, control_map[port]);
  }
}

}  // namespace rtbot

#endif  // STATE_SERIALIZER_H