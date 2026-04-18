#ifndef OPERATOR_H
#define OPERATOR_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>

#include <nlohmann/json.hpp>

#include "rtbot/Base64.h"
#include "rtbot/Message.h"
#include "rtbot/StateSerializer.h"
#include "rtbot/telemetry/OpenTelemetry.h"

namespace rtbot {

// Callers with an input queue this size or larger should prefer the batch
// emit_output overload; smaller queues pay more in vector allocation than
// they save in per-connection amortization.
inline constexpr size_t kEmitBatchThreshold = 20;

// Queue of messages for ports
using MessageQueue = std::deque<std::unique_ptr<BaseMessage>>;

// Port information
struct PortInfo {
  MessageQueue queue;
  std::type_index type;
  timestamp_t last_timestamp{std::numeric_limits<timestamp_t>::min()};

  // Constructor
  PortInfo(MessageQueue q, std::type_index t)
      : queue(std::move(q)), type(t) {}

  // Delete copy operations (MessageQueue contains unique_ptr)
  PortInfo(const PortInfo&) = delete;
  PortInfo& operator=(const PortInfo&) = delete;

  // Explicitly default move operations with noexcept for vector reallocation
  PortInfo(PortInfo&&) noexcept = default;
  PortInfo& operator=(PortInfo&&) noexcept = default;
};

// Output ports carry only type metadata: emitted messages go directly to
// connected children's input queues, so there is no output-side queue.
struct OutputPortInfo {
  std::type_index type;
};

enum class PortKind { DATA, CONTROL };

// Base operator class
class Operator {
 public:
  explicit Operator(std::string id) : id_(std::move(id)) {}
  virtual ~Operator() = default;

  virtual std::string type_name() const = 0;

  // Composite operators (Pipeline, TriggerSet) override to expose their
  // internal operators. Returning nullptr — the common case — means "no
  // children", avoiding the dynamic_pointer_cast probes Program used to do
  // on every collect_outputs call.
  virtual const std::map<std::string, std::shared_ptr<Operator>>* children_ops() const {
    return nullptr;
  }

  // Collect operator state as JSON: {"name": "TypeName", "bytes": "base64..."}
  virtual nlohmann::json collect() {
    return {
      {"name", type_name()},
      {"bytes", bytes_to_base64(collect_bytes())}
    };
  }

  // Serialize core operator state to bytes (used internally by collect)
  virtual Bytes collect_bytes() {
    Bytes bytes;

    // Serialize port counts
    size_t data_ports_count = data_ports_.size();
    size_t control_ports_count = control_ports_.size();
    size_t output_ports_count = output_ports_.size();

    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&data_ports_count),
                 reinterpret_cast<const uint8_t*>(&data_ports_count) + sizeof(data_ports_count));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&control_ports_count),
                 reinterpret_cast<const uint8_t*>(&control_ports_count) + sizeof(control_ports_count));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&output_ports_count),
                 reinterpret_cast<const uint8_t*>(&output_ports_count) + sizeof(output_ports_count));
    // Serialize message queues (last_timestamp is not persisted — it's only
    // used for ordering validation under debug and is rederivable from the
    // queue contents on replay).
    for (const auto& port : data_ports_) {
      StateSerializer::serialize_message_queue(bytes, port.queue);
    }
    for (const auto& port : control_ports_) {
      StateSerializer::serialize_message_queue(bytes, port.queue);
    }
    return bytes;
  }

  virtual void restore(Bytes::const_iterator& it) {
    // ---- Read port counts safely ----
    size_t data_ports_count;
    std::memcpy(&data_ports_count, &(*it), sizeof(data_ports_count));
    it += sizeof(size_t);

    size_t control_ports_count;
    std::memcpy(&control_ports_count, &(*it), sizeof(control_ports_count));
    it += sizeof(size_t);

    size_t output_ports_count;
    std::memcpy(&output_ports_count, &(*it), sizeof(output_ports_count));
    it += sizeof(size_t);

    // ---- Validate counts ----
    StateSerializer::validate_port_count(data_ports_count, data_ports_.size(), "Data");
    StateSerializer::validate_port_count(control_ports_count, control_ports_.size(), "Control");
    StateSerializer::validate_port_count(output_ports_count, output_ports_.size(), "Output");

    // ---- Restore message queues ----
    for (auto& port : data_ports_) {
        StateSerializer::deserialize_message_queue(it, port.queue);
    }
    for (auto& port : control_ports_) {
        StateSerializer::deserialize_message_queue(it, port.queue);
    }
  }

  virtual void restore_data_from_json(const nlohmann::json& j) {
    Bytes bytes = base64_to_bytes(j.at("bytes").get<std::string>());
    auto it = bytes.cbegin();
    restore(it);
  }

  // Dynamic port management with type information
  template <typename T>
  void add_data_port() {
    data_ports_.push_back({MessageQueue{}, std::type_index(typeid(T))});
  }

  template <typename T>
  void add_control_port() {
    control_ports_.push_back({MessageQueue{}, std::type_index(typeid(T))});
  }

  template <typename T>
  void add_output_port() {
    output_ports_.push_back({std::type_index(typeid(T))});
  }

  size_t num_data_ports() const { return data_ports_.size(); }
  size_t num_control_ports() const { return control_ports_.size(); }
  size_t num_output_ports() const { return output_ports_.size(); }

  // Runtime port access for data with type checking
  virtual void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index, bool debug = false) {
    RTBOT_PERF_SCOPE(RECEIVE_DATA);
    if (port_index >= data_ports_.size()) {
      throw std::runtime_error("Invalid data port index at " + type_name() + "(" + id_ + ")" + ":" +
                               std::to_string(port_index));
    }

    if (msg->type() != data_ports_[port_index].type) {
      throw std::runtime_error("Type mismatch on data port at " + type_name() + "(" + id_ + ")" + ":" +
                               std::to_string(port_index));
    }

    // Check timestamp ordering (debug only)
    if (debug && msg->time <= data_ports_[port_index].last_timestamp) {
      throw std::runtime_error("Out of order timestamp received at " + type_name() + "(" + id_ + ")" + " port " +
                               std::to_string(port_index) + ". Current timestamp: " + std::to_string(msg->time) +
                               ", Last timestamp: " + std::to_string(data_ports_[port_index].last_timestamp));
    }

    // Update last timestamp
    data_ports_[port_index].last_timestamp = msg->time;

#ifdef RTBOT_INSTRUMENTATION
    RTBOT_RECORD_MESSAGE(id_, type_name(), std::move(msg->clone()));
#endif

    data_ports_[port_index].queue.push_back(std::move(msg));
  }

  // Batch variant — hand a whole port's worth of messages to the operator in
  // one call. Default implementation loops over `messages` and calls
  // receive_data for each. Operators that can skip per-message queueing (e.g.
  // BurstAggregate that runs a vectorized kernel directly over the buffer)
  // override this and bypass the queue entirely.
  virtual void receive_data_batch(std::vector<std::unique_ptr<BaseMessage>>& messages,
                                    size_t port_index, bool debug = false) {
    for (auto& msg : messages) {
      receive_data(std::move(msg), port_index, debug);
    }
  }

  virtual void reset() {
    for (auto& port : data_ports_) {
      port.last_timestamp = std::numeric_limits<timestamp_t>::min();
      port.queue.clear();
    }
    for (auto& port : control_ports_) {
      port.last_timestamp = std::numeric_limits<timestamp_t>::min();
      port.queue.clear();
    }
    for (auto& queue : debug_output_queues_) {
      queue.clear();
    }
  }

  void execute(bool debug=false) {
    SpanScope span_scope{"operator_execute"};
    RTBOT_ADD_ATTRIBUTE("operator.id", id_);

    // Initialize debug queues before process_data so emit_output can write to them
    if (debug) {
      if (debug_output_queues_.size() != num_output_ports()) {
        debug_output_queues_.clear();
        for (size_t i = 0; i < num_output_ports(); i++) {
          debug_output_queues_.emplace_back();
        }
      }
    } else if (debug_output_queues_.size() > 0) {
      debug_output_queues_.clear();
    }

    // Save the caller's mask so recursive execute() calls on this same
    // operator (via recurrent connections like l1 -> ts11 -> l1) do not
    // clobber the caller's in-progress propagation state.
    uint64_t saved_mask = propagated_mask_;
    propagated_mask_ = 0;

    // Process control messages first
    if (num_control_ports() > 0) {
      SpanScope control_scope{"process_control"};
      process_control(debug);
    }

    // Then process data
    if (num_data_ports() > 0) {
      SpanScope data_scope{"process_data"};
      RTBOT_PERF_SCOPE(PROCESS_DATA);
      process_data(debug);
    }

    // Snapshot this call's mask before executing children — a child may
    // recurse back into this->execute() and reset propagated_mask_.
    uint64_t my_mask = propagated_mask_;
    propagated_mask_ = saved_mask;

    for (auto& conn : connections_) {
      if (conn.child && (my_mask & (uint64_t{1} << conn.output_port))) {
        conn.child->execute(debug);
      }
    }
  }

  // Runtime port access for control messages with type checking
  virtual void receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index, bool debug = false) {

    if (port_index >= control_ports_.size()) {
      throw std::runtime_error("Invalid control port index at " + type_name() + "(" + id_ + ")" + ":" +
                               std::to_string(port_index));
    }

    if (msg->type() != control_ports_[port_index].type) {
      throw std::runtime_error("Type mismatch on control port at " + type_name() + "(" + id_ + ")" + ":" +
                               std::to_string(port_index));
    }

    // Check timestamp ordering (debug only)
    if (debug && msg->time <= control_ports_[port_index].last_timestamp) {
      throw std::runtime_error("Out of order timestamp received at " + type_name() + "(" + id_ + ")" +
                               " control port " + std::to_string(port_index) +
                               ". Current timestamp: " + std::to_string(msg->time) +
                               ", Last timestamp: " + std::to_string(control_ports_[port_index].last_timestamp));
    }

    // Update last timestamp
    control_ports_[port_index].last_timestamp = msg->time;

    control_ports_[port_index].queue.push_back(std::move(msg));

  }

  std::shared_ptr<Operator> connect(std::shared_ptr<Operator> child, size_t output_port = 0,
                                    size_t child_port_index = 0, PortKind child_port_kind = PortKind::DATA) {
    if (output_port >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index");
    }

    // Type check based on port kind
    if (child_port_kind == PortKind::DATA) {
      if (child_port_index >= child->data_ports_.size()) {
        throw std::runtime_error("Invalid child data port index " + std::to_string(child_port_index) +
                                 ", available data ports: " + std::to_string(child->data_ports_.size()) +
                                 ", found while connecting " + id_ + ":" + std::to_string(output_port) + " -> " +
                                 child->id_ + ":" + std::to_string(child_port_index));
      }
      if (output_ports_[output_port].type != child->data_ports_[child_port_index].type) {
        throw std::runtime_error(
            "Input port type mismatch in operator connection " + id_ + ":" + std::to_string(output_port) + " -> " +
            child->id_ + ":" + std::to_string(child_port_index) + " expected " +
            child->data_ports_[child_port_index].type.name() + " but got " + output_ports_[output_port].type.name());
      }
    } else {
      if (child_port_index >= child->control_ports_.size()) {
        throw std::runtime_error("Invalid child control port index " + std::to_string(child_port_index) +
                                 ", available control ports: " + std::to_string(child->control_ports_.size()) +
                                 ", found while connecting " + id_ + ":" + std::to_string(output_port) + " -> " +
                                 child->id_ + ":" + std::to_string(child_port_index));
      }
      if (output_ports_[output_port].type != child->control_ports_[child_port_index].type) {
        throw std::runtime_error(
            "Control port type mismatch in operator connection " + id_ + ":" + std::to_string(output_port) + " -> " +
            child->id_ + ":" + std::to_string(child_port_index) + " expected " +
            child->control_ports_[child_port_index].type.name() + " but got " + output_ports_[output_port].type.name());
      }
    }

    Connection conn{child.get(), output_port, child_port_index, child_port_kind, nullptr};
    auto& pi = (child_port_kind == PortKind::DATA)
                   ? child->data_ports_[child_port_index]
                   : child->control_ports_[child_port_index];
    conn.sink_queue = &pi.queue;
    conn.sink_last_ts = &pi.last_timestamp;
    size_t conn_idx = connections_.size();
    connections_.push_back(std::move(conn));

    // Register the reverse edge on the child so downstream code (e.g.
    // Program::collect_outputs) can discover who feeds each input port.
    if (child_port_kind == PortKind::DATA) {
      if (child->inbound_data_refs_.size() < child->data_ports_.size())
        child->inbound_data_refs_.resize(child->data_ports_.size());
      child->inbound_data_refs_[child_port_index].push_back({this, conn_idx});
    } else {
      if (child->inbound_control_refs_.size() < child->control_ports_.size())
        child->inbound_control_refs_.resize(child->control_ports_.size());
      child->inbound_control_refs_[child_port_index].push_back({this, conn_idx});
    }

    if (output_port >= conn_count_per_port_.size()) {
      conn_count_per_port_.resize(output_port + 1, 0);
    }
    ++conn_count_per_port_[output_port];
    return child;
  }

  // Get port type
  std::type_index get_data_port_type(size_t port_index) const {
    if (port_index >= data_ports_.size()) {
      throw std::runtime_error("Invalid data port index for data queue");
    }
    return data_ports_[port_index].type;
  }

  std::type_index get_control_port_type(size_t port_index) const {
    if (port_index >= control_ports_.size()) {
      throw std::runtime_error("Invalid control port index for control queue");
    }
    return control_ports_[port_index].type;
  }

  std::type_index get_output_port_type(size_t port_index) const {
    if (port_index >= output_ports_.size()) {
      throw std::runtime_error("Invalid output port index");
    }
    return output_ports_[port_index].type;
  }

  const std::string& id() const { return id_; }

  bool equals(const Operator& other) const {
      // Compare IDs
      if (id_ != other.id_) return false;
      if (type_name() != other.type_name()) return false;

      // Compare number of ports
      if (data_ports_.size() != other.data_ports_.size()) return false;
      if (control_ports_.size() != other.control_ports_.size()) return false;
      if (output_ports_.size() != other.output_ports_.size()) return false;

      // Compare data port types and buffered queues
      for (size_t i = 0; i < data_ports_.size(); ++i) {
        if (data_ports_[i].type != other.data_ports_[i].type) return false;

        if (data_ports_[i].queue.size() != other.data_ports_[i].queue.size()) return false;
        for (size_t j = 0; j < data_ports_[i].queue.size(); ++j) {
            if (data_ports_[i].queue[j]->hash() != other.data_ports_[i].queue[j]->hash()) return false;
            if (data_ports_[i].queue[j]->time != other.data_ports_[i].queue[j]->time) return false;
        }
      }

      // Compare control port types and buffered queues
      for (size_t i = 0; i < control_ports_.size(); ++i) {
        if (control_ports_[i].type != other.control_ports_[i].type) return false;

        if (control_ports_[i].queue.size() != other.control_ports_[i].queue.size()) return false;
        for (size_t j = 0; j < control_ports_[i].queue.size(); ++j) {
            if (control_ports_[i].queue[j]->hash() != other.control_ports_[i].queue[j]->hash()) return false;
            if (control_ports_[i].queue[j]->time != other.control_ports_[i].queue[j]->time) return false;
        }
      }

      return true;
  }
  
  bool operator==(const Operator& other) const {
    return equals(other);
  }

  bool operator!=(const Operator& other) const {
    return !(*this == other);
  }

  // Access to port queues for derived classes
  MessageQueue& get_data_queue(size_t port_index) {
    if (port_index >= data_ports_.size()) {
      throw std::runtime_error("Invalid data port index for data queue");
    }
    return data_ports_[port_index].queue;
  }

  MessageQueue& get_control_queue(size_t port_index) {
    if (port_index >= control_ports_.size()) {
      throw std::runtime_error("Invalid control port index for control queue");
    }
    return control_ports_[port_index].queue;
  }

  void clear_debug_output_queues() {
    for (auto& queue : debug_output_queues_) {
      queue.clear();
    }
  }

  MessageQueue& get_debug_output_queue(size_t port_index) {
    static MessageQueue empty;
    if (port_index >= debug_output_queues_.size()) {
      return empty;
    }
    return debug_output_queues_[port_index];
  }

  // Outbound connection record: one entry per (this_output_port → child_port)
  // edge. Public so downstream operators can read provenance from their
  // inbound refs (see InboundRef / inbound_data_refs()).
  struct Connection {
    // B.2: raw pointer — Program owns operators via shared_ptr for its full
    // lifetime, so this never dangles in the steady state. Previously a
    // weak_ptr, whose atomic .lock() was ~3% of CPU on the hot path.
    Operator* child{nullptr};
    size_t output_port;
    size_t child_input_port;
    PortKind child_port_kind{PortKind::DATA};
    // Direct pointer to the child's input queue: emit_output pushes here,
    // bypassing the virtual receive_data/receive_control. Always set at
    // connect() time — every downstream operator in the graph receives via
    // this fast path (Input, the only operator with custom receive logic,
    // is always the graph entry and never a connection child).
    MessageQueue* sink_queue{nullptr};
    // Cached pointer to the child port's last_timestamp for debug-mode
    // ordering validation.
    timestamp_t* sink_last_ts{nullptr};
  };

  // Reverse-edge reference: "there's a connection feeding one of my input
  // ports — it lives at parent->connections_[conn_index]". Stored by index
  // rather than by pointer so connections_ reallocations during setup don't
  // invalidate us.
  struct InboundRef {
    Operator* parent{nullptr};
    size_t conn_index{0};
  };

  // Read-only access to an outbound connection by index. Used by consumers
  // of InboundRef to read source provenance (output_port, etc.).
  const Connection& get_connection(size_t conn_index) const {
    return connections_[conn_index];
  }

  const std::vector<InboundRef>& inbound_data_refs(size_t port_index) const {
    static const std::vector<InboundRef> empty;
    if (port_index >= inbound_data_refs_.size()) return empty;
    return inbound_data_refs_[port_index];
  }

  const std::vector<InboundRef>& inbound_control_refs(size_t port_index) const {
    static const std::vector<InboundRef> empty;
    if (port_index >= inbound_control_refs_.size()) return empty;
    return inbound_control_refs_[port_index];
  }

 protected:
  virtual void process_data(bool debug) = 0;
  virtual void process_control(bool debug=false) {};

  // Push a message out of the given output port. Delivers directly to
  // connected children's input/control queues. Also copies to debug queues
  // when debug mode is active.
  void emit_output(size_t port_index, std::unique_ptr<BaseMessage> msg, bool debug = false) {
    if (debug) {
      debug_output_queues_[port_index].push_back(msg->clone());
    }

#ifdef RTBOT_INSTRUMENTATION
    RTBOT_RECORD_OPERATOR_OUTPUT(id_, type_name(), port_index, msg->clone());
#endif

    // Cached count of connections on this port (maintained by connect()).
    size_t remaining = (port_index < conn_count_per_port_.size())
                           ? conn_count_per_port_[port_index]
                           : 0;

    if (remaining == 0) return;

    propagated_mask_ |= (uint64_t{1} << port_index);

    for (auto& conn : connections_) {
      if (conn.output_port != port_index) continue;
      --remaining;

      std::unique_ptr<BaseMessage> msg_to_send;
#if defined(RTBOT_INSTRUMENTATION)
      msg_to_send = msg->clone();
      RTBOT_RECORD_MESSAGE_SENT(id_, type_name(), std::to_string(port_index),
                                conn.child->id(), conn.child->type_name(),
                                std::to_string(conn.child_input_port),
                                conn.child_port_kind == PortKind::DATA ? "" : "[c]",
                                msg->clone());
#else
      if (remaining == 0 && !debug) {
        msg_to_send = std::move(msg);
      } else {
        RTBOT_PERF_SCOPE(EMIT_CLONE);
        msg_to_send = msg->clone();
      }
#endif

      {
        RTBOT_PERF_SCOPE(EMIT_DISPATCH);
        if (debug) {
          if (msg_to_send->time <= *conn.sink_last_ts) {
            throw std::runtime_error(
                "Message time out of order on port " +
                std::to_string(conn.child_input_port) +
                ". Current time: " + std::to_string(msg_to_send->time) +
                ", Last timestamp: " + std::to_string(*conn.sink_last_ts));
          }
          *conn.sink_last_ts = msg_to_send->time;
        }
        conn.sink_queue->push_back(std::move(msg_to_send));
      }
    }
  }

  // Batch overload: drain an entire vector of messages out of `port_index`
  // in one pass. Connections are iterated once per batch (vs once per
  // message in the single-msg overload), so the sink_queue tail node stays
  // hot across messages to the same child.
  void emit_output(size_t port_index,
                   std::vector<std::unique_ptr<BaseMessage>> msgs,
                   bool debug = false) {
    if (msgs.empty()) return;

    if (debug) {
      for (const auto& m : msgs) {
        debug_output_queues_[port_index].push_back(m->clone());
      }
    }

#ifdef RTBOT_INSTRUMENTATION
    for (const auto& m : msgs) {
      RTBOT_RECORD_OPERATOR_OUTPUT(id_, type_name(), port_index, m->clone());
    }
#endif

    size_t total_conns = (port_index < conn_count_per_port_.size())
                             ? conn_count_per_port_[port_index]
                             : 0;
    if (total_conns == 0) return;

    propagated_mask_ |= (uint64_t{1} << port_index);

    size_t seen = 0;
    for (auto& conn : connections_) {
      if (conn.output_port != port_index) continue;
      ++seen;
      const bool can_move = (seen == total_conns) && !debug;

      for (size_t i = 0; i < msgs.size(); ++i) {
        std::unique_ptr<BaseMessage> msg_to_send;
#if defined(RTBOT_INSTRUMENTATION)
        msg_to_send = msgs[i]->clone();
        RTBOT_RECORD_MESSAGE_SENT(id_, type_name(), std::to_string(port_index),
                                  conn.child->id(), conn.child->type_name(),
                                  std::to_string(conn.child_input_port),
                                  conn.child_port_kind == PortKind::DATA ? "" : "[c]",
                                  msgs[i]->clone());
#else
        if (can_move) {
          msg_to_send = std::move(msgs[i]);
        } else {
          RTBOT_PERF_SCOPE(EMIT_CLONE);
          msg_to_send = msgs[i]->clone();
        }
#endif
        if (debug) {
          if (msg_to_send->time <= *conn.sink_last_ts) {
            throw std::runtime_error(
                "Message time out of order on port " +
                std::to_string(conn.child_input_port) +
                ". Current time: " + std::to_string(msg_to_send->time) +
                ", Last timestamp: " + std::to_string(*conn.sink_last_ts));
          }
          *conn.sink_last_ts = msg_to_send->time;
        }
        conn.sink_queue->push_back(std::move(msg_to_send));
      }
    }
  }

  bool sync_data_inputs() {

    if (data_ports_.empty()) return false;

    while (true) {
      // If any queue is empty, sync not possible
      for (auto& port : data_ports_) {
        if (port.queue.empty())
          return false;
      }

      // Find min and max front timestamps
      timestamp_t min_time = data_ports_.front().queue.front()->time;
      timestamp_t max_time = min_time;

      for (auto& port : data_ports_) {
        timestamp_t t = port.queue.front()->time;
        if (t < min_time) min_time = t;
        if (t > max_time) max_time = t;
      }

      // All equal → synchronized
      if (min_time == max_time)
        return true;

      // Pop all queues that have the oldest front timestamp
      for (auto& port : data_ports_) {
        if (!port.queue.empty() && port.queue.front()->time == min_time)
          port.queue.pop_front();
      }

      // If any queue now empty → cannot sync
      for (auto& port : data_ports_) {
        if (port.queue.empty())
          return false;
      }
    }
    return false;  
  }

  bool sync_control_inputs() {

    if (control_ports_.empty()) return false;

    while (true) {
      // If any queue is empty, sync not possible
      for (auto& port : control_ports_) {
        if (port.queue.empty())
          return false;
      }

      // Find min and max front timestamps
      timestamp_t min_time = control_ports_.front().queue.front()->time;
      timestamp_t max_time = min_time;

      for (auto& port : control_ports_) {
        timestamp_t t = port.queue.front()->time;
        if (t < min_time) min_time = t;
        if (t > max_time) max_time = t;
      }

      // All equal → synchronized
      if (min_time == max_time)
        return true;

      // Pop all queues that have the oldest front timestamp
      for (auto& port : control_ports_) {
        if (!port.queue.empty() && port.queue.front()->time == min_time)
          port.queue.pop_front();
      }

      // If any queue now empty → cannot sync
      for (auto& port : control_ports_) {
        if (port.queue.empty())
          return false;
      }
    }
    return false;  
  }

  std::string id_;
  std::vector<PortInfo> data_ports_;
  std::vector<PortInfo> control_ports_;
  std::vector<OutputPortInfo> output_ports_;
  std::deque<MessageQueue> debug_output_queues_;
  std::vector<Connection> connections_;
  // Reverse index: input port → list of inbound connections feeding it.
  // Stored as (parent_op*, conn_index) so upstream's connections_ vector can
  // reallocate during setup without invalidating these refs. Sized lazily in
  // connect() on the child side.
  std::vector<std::vector<InboundRef>> inbound_data_refs_;
  std::vector<std::vector<InboundRef>> inbound_control_refs_;
  // Cached count of connections per output port, maintained by connect().
  // Lets emit_output skip the count-pass loop.
  std::vector<uint16_t> conn_count_per_port_;
  uint64_t propagated_mask_{0};
};

}  // namespace rtbot

#endif  // OPERATOR_H
