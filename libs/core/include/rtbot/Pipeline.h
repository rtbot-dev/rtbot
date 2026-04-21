#ifndef PIPELINE_H
#define PIPELINE_H

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Logger.h"
#include "rtbot/Collector.h"
#include "rtbot/CompositeConnection.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"

namespace rtbot {

class Pipeline : public Operator {
 public:

  static constexpr size_t kSegmentStackSize = 64;

  Pipeline(std::string id, const std::vector<std::string>& input_port_types,
           const std::vector<std::string>& output_port_types,
           std::vector<double> segment_bytecode = {},
           std::vector<double> segment_constants = {})
      : Operator(std::move(id)),
        segment_bytecode_(std::move(segment_bytecode)),
        segment_constants_(std::move(segment_constants)) {
    // Configure input ports
    for (const auto& type : input_port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown input port type: " + type);
      }
      PortType::add_port(*this, type, true, false, false);
      input_port_types_.push_back(type);
    }

    // Configure output ports
    for (const auto& type : output_port_types) {
      if (!PortType::is_valid_port_type(type)) {
        throw std::runtime_error("Unknown output port type: " + type);
      }
      PortType::add_port(*this, type, false, false, true);
      output_port_types_.push_back(type);
    }

    // Segment bytecode mode requires first input port to be VECTOR_NUMBER
    if (!segment_bytecode_.empty() && (input_port_types_.empty() ||
        input_port_types_[0] != PortType::VECTOR_NUMBER)) {
      throw std::runtime_error(
          "Segment bytecode mode requires first input port to be VECTOR_NUMBER");
    }

    // Validate segment bytecode stack usage at construction so the hot-path
    // interpreter can use a fixed-size stack without per-op bounds checks.
    if (!segment_bytecode_.empty()) {
      validate_segment_bytecode_stack_();
    }

    // Control port: only created when no segment bytecode is provided
    // When segment_bytecode is non-empty, the segment key is computed internally
    if (segment_bytecode_.empty()) {
      add_control_port<NumberData>();
    }
  }

  // Get port configurations
  const std::vector<std::string>& get_input_port_types() const { return input_port_types_; }
  const std::vector<std::string>& get_output_port_types() const { return output_port_types_; }
  const std::map<std::string, std::shared_ptr<Operator>>& get_operators() const { return operators_; }
  const std::map<std::string, std::shared_ptr<Operator>>* children_ops() const override { return &operators_; }
  const std::vector<CompositeConnection>& get_connections() const { return connections_; }
  const std::string& get_entry_operator_id() const { return entry_operator_->id(); }
  const std::map<std::string, std::vector<std::pair<size_t, size_t>>>& get_output_mappings() const {
    return output_mappings_;
  }
  const std::vector<double>& get_segment_bytecode() const { return segment_bytecode_; }
  const std::vector<double>& get_segment_constants() const { return segment_constants_; }
  bool has_segment_bytecode() const { return !segment_bytecode_.empty(); }

  // API for configuring the pipeline
  void register_operator(std::shared_ptr<Operator> op) { operators_[op->id()] = std::move(op); }

  void set_entry(const std::string& op_id) {
    auto it = operators_.find(op_id);
    if (it == operators_.end()) {
      throw std::runtime_error("Entry operator not found: " + op_id);
    }
    if (it->second->num_data_ports() >= num_data_ports()) {
      entry_operator_ = it->second;
      RTBOT_LOG_DEBUG("Setting entry operator: ", op_id);
    } else {
      throw std::runtime_error("Entry operator has less data ports that the pipeline: " + op_id);
    }
  }

  void add_output_mapping(const std::string& op_id, size_t op_port, size_t pipeline_port) {
    auto it = operators_.find(op_id);
    if (it == operators_.end()) {
      throw std::runtime_error("Output operator not found: " + op_id);
    }
    if (pipeline_port >= num_output_ports()) {
      throw std::runtime_error("Invalid pipeline output port: " + std::to_string(pipeline_port));
    }
    RTBOT_LOG_DEBUG("Adding output mapping: ", op_id, ":", op_port, " -> ", pipeline_port);
    output_mappings_[op_id].emplace_back(op_port, pipeline_port);

    // Attach a collector to capture this operator's output on this port
    std::string collector_id = op_id + "_collector_" + std::to_string(op_port);
    if (output_collectors_.find(collector_id) == output_collectors_.end()) {
      auto collector = std::make_shared<Collector>(
          collector_id, std::vector<std::string>{output_port_types_[pipeline_port]});
      it->second->connect(collector, op_port, 0);
      output_collectors_[collector_id] = collector;
    }
    collector_keys_[{op_id, op_port}] = collector_id;
    rebuild_output_cache_();
  }

  using Operator::connect;

  void connect(const std::shared_ptr<Operator>& from, const std::shared_ptr<Operator>& to, size_t from_port = 0,
               size_t to_port = 0) {
    if (!from || !to) {
      throw std::runtime_error("Pipeline: null operator passed to connect");
    }
    const std::string& from_id = from->id();
    const std::string& to_id = to->id();
    if (operators_.find(from_id) == operators_.end() || operators_.find(to_id) == operators_.end()) {
      throw std::runtime_error("Pipeline: invalid operator reference in connection from " + from_id + " to " + to_id);
    }

    RTBOT_LOG_DEBUG("Connecting operators: ", from_id, " -> ", to_id);
    from->connect(to, from_port, to_port);
    connections_.push_back({from_id, to_id, from_port, to_port});
  }

  void reset() override {
    RTBOT_LOG_DEBUG("Resetting pipeline");
    for (auto& [_, op] : operators_) {
      op->reset();
    }
    has_key_ = false;
    current_key_ = 0.0;
    last_output_buffer_.clear();
  }


  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();

    // Serialize segment state
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&has_key_),
                 reinterpret_cast<const uint8_t*>(&has_key_) + sizeof(has_key_));
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&current_key_),
                 reinterpret_cast<const uint8_t*>(&current_key_) + sizeof(current_key_));

    // Serialize output buffer
    size_t buffer_size = last_output_buffer_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&buffer_size),
                 reinterpret_cast<const uint8_t*>(&buffer_size) + sizeof(buffer_size));

    for (const auto& [port, msg] : last_output_buffer_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&port),
                   reinterpret_cast<const uint8_t*>(&port) + sizeof(port));
      Bytes msg_bytes = msg->serialize();
      size_t msg_size = msg_bytes.size();
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&msg_size),
                   reinterpret_cast<const uint8_t*>(&msg_size) + sizeof(msg_size));
      bytes.insert(bytes.end(), msg_bytes.begin(), msg_bytes.end());
    }

    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    Operator::restore(it);

    // Restore segment state
    std::memcpy(&has_key_, &(*it), sizeof(has_key_));
    it += sizeof(has_key_);
    std::memcpy(&current_key_, &(*it), sizeof(current_key_));
    it += sizeof(current_key_);

    // Restore output buffer
    size_t buffer_size;
    std::memcpy(&buffer_size, &(*it), sizeof(buffer_size));
    it += sizeof(buffer_size);

    last_output_buffer_.clear();
    for (size_t i = 0; i < buffer_size; ++i) {
      size_t port;
      std::memcpy(&port, &(*it), sizeof(port));
      it += sizeof(port);

      size_t msg_size;
      std::memcpy(&msg_size, &(*it), sizeof(msg_size));
      it += sizeof(msg_size);

      Bytes msg_bytes(it, it + msg_size);
      last_output_buffer_[port] = BaseMessage::deserialize(msg_bytes);
      it += msg_size;
    }
  }

  nlohmann::json collect() override {
    nlohmann::json result = {
      {"name", type_name()},
      {"bytes", bytes_to_base64(collect_bytes())}
    };

    nlohmann::json content;
    for (const auto& [op_id, op] : operators_) {
      content[op_id] = op->collect();
    }
    result["content"] = content;

    return result;
  }

  void restore_data_from_json(const nlohmann::json& j) override {
    Bytes bytes = base64_to_bytes(j.at("bytes").get<std::string>());
    auto it = bytes.cbegin();
    restore(it);

    const auto& content = j.at("content");
    for (auto& [op_id, op] : operators_) {
      op->restore_data_from_json(content.at(op_id));
    }
  }

  std::string type_name() const override { return "Pipeline"; }

  bool equals(const Pipeline& other) const {
    if (input_port_types_ != other.input_port_types_) return false;
    if (output_port_types_ != other.output_port_types_) return false;
    if (segment_bytecode_ != other.segment_bytecode_) return false;
    if (segment_constants_ != other.segment_constants_) return false;
    if (output_mappings_ != other.output_mappings_) return false;
    if ((bool)entry_operator_ != (bool)other.entry_operator_) return false;
    if (entry_operator_ && other.entry_operator_) {
      if (*entry_operator_ != *other.entry_operator_)
          return false;
    }
    if (operators_.size() != other.operators_.size()) return false;

    for (const auto& [key, op1] : operators_) {
        auto it = other.operators_.find(key);
        if (it == other.operators_.end()) return false;
        const auto& op2 = it->second;
        if (!op1 || !op2) return false;
        else if (*op1 != *op2) return false;
    }

    // Compare segment state
    if (has_key_ != other.has_key_) return false;
    if (has_key_ && current_key_ != other.current_key_) return false;
    if (last_output_buffer_.size() != other.last_output_buffer_.size()) return false;
    for (const auto& [port, msg] : last_output_buffer_) {
      auto oit = other.last_output_buffer_.find(port);
      if (oit == other.last_output_buffer_.end()) return false;
      if (msg->hash() != oit->second->hash()) return false;
      if (msg->time != oit->second->time) return false;
    }

    return Operator::equals(other);
  }

  bool operator==(const Pipeline& other) const {
    return equals(other);
  }

  bool operator!=(const Pipeline& other) const {
    return !(*this == other);
  }

 protected:
  void process_data(bool debug=false) override {
    if (!entry_operator_) {
      throw std::runtime_error("Pipeline entry point not configured");
    }
    if (!segment_bytecode_.empty()) {
      process_data_with_segment_bytecode(debug);
    } else {
      process_data_with_control_port(debug);
    }
  }

 private:
  // Control-port mode: original process_data logic (unchanged)
  void process_data_with_control_port(bool debug) {
    auto& control_queue = get_control_queue(0);

    while (!control_queue.empty()) {
      // Check all data ports have at least one message
      bool all_data_ready = true;
      for (size_t i = 0; i < num_data_ports(); ++i) {
        if (get_data_queue(i).empty()) {
          all_data_ready = false;
          break;
        }
      }
      if (!all_data_ready) break;

      timestamp_t ctrl_time = control_queue.front()->time;

      // Find the minimum data timestamp across all data ports
      timestamp_t min_data_time = get_data_queue(0).front()->time;
      for (size_t i = 1; i < num_data_ports(); ++i) {
        timestamp_t t = get_data_queue(i).front()->time;
        if (t < min_data_time) min_data_time = t;
      }

      // Sync: discard older messages from whichever side is behind
      if (min_data_time < ctrl_time) {
        RTBOT_LOG_DEBUG("Pipeline: discarding data at t=", min_data_time, " (older than control t=", ctrl_time, ")");
        for (size_t i = 0; i < num_data_ports(); ++i) {
          if (!get_data_queue(i).empty() && get_data_queue(i).front()->time == min_data_time) {
            get_data_queue(i).pop_front();
          }
        }
        continue;
      }
      if (ctrl_time < min_data_time) {
        RTBOT_LOG_DEBUG("Pipeline: discarding control at t=", ctrl_time, " (older than data t=", min_data_time, ")");
        control_queue.pop_front();
        continue;
      }

      // Timestamps match: process this pair
      double new_key = static_cast<const Message<NumberData>*>(control_queue.front().get())->data.value;
      timestamp_t boundary_time = ctrl_time;

      if (has_key_ && new_key != current_key_) {
        // Key changed: emit buffer with boundary timestamp, reset internals, start new segment
        emit_buffer(boundary_time, debug);
        reset_internals();
      }
      if (!has_key_) has_key_ = true;
      current_key_ = new_key;
      forward_and_buffer(debug);

      // Consume the processed messages
      for (size_t i = 0; i < num_data_ports(); ++i) {
        get_data_queue(i).pop_front();
      }
      control_queue.pop_front();
    }
  }

  // Segment-bytecode mode: evaluate bytecode on incoming vector, no control port
  void process_data_with_segment_bytecode(bool debug) {
    while (true) {
      // Check all data ports have at least one message
      bool all_data_ready = true;
      for (size_t i = 0; i < num_data_ports(); ++i) {
        if (get_data_queue(i).empty()) {
          all_data_ready = false;
          break;
        }
      }
      if (!all_data_ready) break;

      // Evaluate segment bytecode on the first data port's vector to get segment key
      const auto* vec_msg = static_cast<const Message<VectorNumberData>*>(get_data_queue(0).front().get());
      const auto& vec = *vec_msg->data.values;
      double new_key = evaluate_segment_bytecode(vec);
      timestamp_t boundary_time = vec_msg->time;

      if (has_key_ && new_key != current_key_) {
        // Key changed: emit buffer with boundary timestamp, reset internals, start new segment
        emit_buffer(boundary_time);
        reset_internals();
      }
      if (!has_key_) has_key_ = true;
      current_key_ = new_key;
      forward_and_buffer(debug);

      // Pop consumed messages from data queues
      for (size_t i = 0; i < num_data_ports(); ++i) {
        get_data_queue(i).pop_front();
      }
    }
  }

  // Symbolically execute the segment bytecode to verify that the RPN stack
  // never exceeds kSegmentStackSize and never underflows. Runs once at
  // construction so evaluate_segment_bytecode can stay branchless on the
  // hot path.
  void validate_segment_bytecode_stack_() const {
    const double* bc = segment_bytecode_.data();
    const size_t bc_size = segment_bytecode_.size();
    size_t sp = 0;
    size_t max_sp = 0;
    size_t pc = 0;
    auto need = [&](size_t n) {
      if (sp < n) {
        throw std::runtime_error(
            "Pipeline segment bytecode: stack underflow at pc=" +
            std::to_string(pc));
      }
    };
    auto push = [&]() {
      if (++sp > max_sp) max_sp = sp;
      if (sp > kSegmentStackSize) {
        throw std::runtime_error(
            "Pipeline segment bytecode: stack overflow (depth " +
            std::to_string(sp) + " > " +
            std::to_string(kSegmentStackSize) + ") at pc=" +
            std::to_string(pc));
      }
    };
    while (pc < bc_size) {
      int opcode = static_cast<int>(bc[pc++]);
      switch (opcode) {
        case 0: case 1: /* INPUT, CONST */
          ++pc; push(); break;
        case 2: case 3: case 4: case 5: case 6: /* ADD..DIV, POW */
        case 26: case 27: case 28: case 29: case 30: case 31: /* cmp */
        case 32: case 33: /* AND, OR */
          need(2); --sp; break;
        case 7: case 8: case 9: case 10: case 11: case 12:
        case 13: case 14: case 15: case 16: case 17: case 18:
        case 19: case 34: /* unary */
          need(1); break;
        case 20: /* END */
          need(1); return;
        default:
          throw std::runtime_error(
              "Pipeline segment bytecode: unknown opcode " +
              std::to_string(opcode) + " at pc=" + std::to_string(pc - 1));
      }
    }
    throw std::runtime_error("Pipeline segment bytecode: missing END opcode");
  }

  // RPN bytecode interpreter for segment key evaluation.
  // Evaluates against a vector — INPUT reads vec[argument] directly.
  // Only stateless opcodes (0-20, 26-34) are supported; stateful aggregate
  // opcodes (21-25: CUMSUM, COUNT, MAX_AGG, MIN_AGG, STATE_LOAD) are not
  // available since the segment expression must be a pure function of the
  // current vector. Segment keys are compared with exact floating-point
  // equality, so expressions should produce integer-valued results
  // (e.g., via FLOOR, comparisons producing 1.0/0.0).
  double evaluate_segment_bytecode(const std::vector<double>& vec) const {
    const double* bc = segment_bytecode_.data();
    const size_t bc_size = segment_bytecode_.size();
    const double* consts = segment_constants_.data();

    double stack[kSegmentStackSize];
    size_t sp = 0;
    size_t pc = 0;

    while (pc < bc_size) {
      int opcode = static_cast<int>(bc[pc++]);

      switch (opcode) {
        case 0 /* INPUT */: {
          stack[sp++] = vec[static_cast<int>(bc[pc++])];
          break;
        }
        case 1 /* CONST */: {
          stack[sp++] = consts[static_cast<int>(bc[pc++])];
          break;
        }
        case 2 /* ADD */: {
          double b = stack[--sp];
          stack[sp - 1] += b;
          break;
        }
        case 3 /* SUB */: {
          double b = stack[--sp];
          stack[sp - 1] -= b;
          break;
        }
        case 4 /* MUL */: {
          double b = stack[--sp];
          stack[sp - 1] *= b;
          break;
        }
        case 5 /* DIV */: {
          double b = stack[--sp];
          stack[sp - 1] /= b;
          break;
        }
        case 6 /* POW */: {
          double b = stack[--sp];
          double a = stack[--sp];
          stack[sp++] = std::pow(a, b);
          break;
        }
        case 7 /* ABS */: {
          stack[sp - 1] = std::abs(stack[sp - 1]);
          break;
        }
        case 8 /* SQRT */: {
          stack[sp - 1] = std::sqrt(stack[sp - 1]);
          break;
        }
        case 9 /* LOG */: {
          stack[sp - 1] = std::log(stack[sp - 1]);
          break;
        }
        case 10 /* LOG10 */: {
          stack[sp - 1] = std::log10(stack[sp - 1]);
          break;
        }
        case 11 /* EXP */: {
          stack[sp - 1] = std::exp(stack[sp - 1]);
          break;
        }
        case 12 /* SIN */: {
          stack[sp - 1] = std::sin(stack[sp - 1]);
          break;
        }
        case 13 /* COS */: {
          stack[sp - 1] = std::cos(stack[sp - 1]);
          break;
        }
        case 14 /* TAN */: {
          stack[sp - 1] = std::tan(stack[sp - 1]);
          break;
        }
        case 15 /* SIGN */: {
          double v = stack[sp - 1];
          stack[sp - 1] = (v > 0.0) ? 1.0 : (v < 0.0) ? -1.0 : 0.0;
          break;
        }
        case 16 /* FLOOR */: {
          stack[sp - 1] = std::floor(stack[sp - 1]);
          break;
        }
        case 17 /* CEIL */: {
          stack[sp - 1] = std::ceil(stack[sp - 1]);
          break;
        }
        case 18 /* ROUND */: {
          stack[sp - 1] = std::round(stack[sp - 1]);
          break;
        }
        case 19 /* NEG */: {
          stack[sp - 1] = -stack[sp - 1];
          break;
        }
        case 20 /* END */: {
          // Return the top of stack as the segment key
          return stack[--sp];
        }
        case 26 /* GT */: {
          double b = stack[--sp];
          stack[sp - 1] = (stack[sp - 1] > b) ? 1.0 : 0.0;
          break;
        }
        case 27 /* GTE */: {
          double b = stack[--sp];
          stack[sp - 1] = (stack[sp - 1] >= b) ? 1.0 : 0.0;
          break;
        }
        case 28 /* LT */: {
          double b = stack[--sp];
          stack[sp - 1] = (stack[sp - 1] < b) ? 1.0 : 0.0;
          break;
        }
        case 29 /* LTE */: {
          double b = stack[--sp];
          stack[sp - 1] = (stack[sp - 1] <= b) ? 1.0 : 0.0;
          break;
        }
        case 30 /* EQ */: {
          double b = stack[--sp];
          stack[sp - 1] = (stack[sp - 1] == b) ? 1.0 : 0.0;
          break;
        }
        case 31 /* NEQ */: {
          double b = stack[--sp];
          stack[sp - 1] = (stack[sp - 1] != b) ? 1.0 : 0.0;
          break;
        }
        case 32 /* AND */: {
          double b = stack[--sp];
          double a = stack[--sp];
          stack[sp++] = (a != 0.0 && b != 0.0) ? 1.0 : 0.0;
          break;
        }
        case 33 /* OR */: {
          double b = stack[--sp];
          double a = stack[--sp];
          stack[sp++] = (a != 0.0 || b != 0.0) ? 1.0 : 0.0;
          break;
        }
        case 34 /* NOT */: {
          stack[sp - 1] = (stack[sp - 1] == 0.0) ? 1.0 : 0.0;
          break;
        }
        default:
          throw std::runtime_error(
              "Pipeline: unknown segment bytecode opcode " + std::to_string(opcode));
      }
    }
    // Missing END opcode is always a bytecode authoring bug
    throw std::runtime_error("Pipeline segment bytecode: missing END opcode");
  }

  // Forward data from all input ports to the entry operator, execute the mesh,
  // and buffer any internal outputs (don't emit to pipeline output). The hot
  // path walks a flat cache built at setup time; the three std::map lookups
  // this replaces were showing ~5% of total CPU on the IMS profile.
  void forward_and_buffer(bool debug) {
    for (auto& entry : output_mapping_cache_) {
      entry.collector->reset();
    }

    for (size_t i = 0; i < num_data_ports(); ++i) {
      entry_operator_->receive_data(std::move(get_data_queue(i).front()), i);
    }
    entry_operator_->execute(debug);

    for (const auto& entry : output_mapping_cache_) {
      auto& source_queue = entry.collector->get_data_queue(0);
      if (!source_queue.empty()) {
        last_output_buffer_[entry.pipeline_port] = source_queue.back()->clone();
      }
    }
  }

  // Emit buffered output to pipeline output ports, stamped with boundary timestamp
  void emit_buffer(timestamp_t boundary_time, bool debug = false) {
    for (auto& [pipeline_port, msg] : last_output_buffer_) {
      if (pipeline_port < num_output_ports()) {
        msg->time = boundary_time;
        RTBOT_LOG_DEBUG("Pipeline emitting buffer on port ", pipeline_port, " at time ", boundary_time);
        emit_output(pipeline_port, std::move(msg), debug);
      }
    }
    last_output_buffer_.clear();
  }

  // Reset only internal operators (not Pipeline's own segment state)
  void reset_internals() {
    for (auto& [_, op] : operators_) {
      op->reset();
    }
  }

  // Flat cache rebuilt whenever add_output_mapping runs. Lets
  // forward_and_buffer walk raw pointers instead of three nested std::map
  // lookups per emitted output per segment boundary.
  struct OutputMappingFlat {
    Collector* collector;
    size_t pipeline_port;
  };
  void rebuild_output_cache_() {
    output_mapping_cache_.clear();
    for (const auto& [op_id, mappings] : output_mappings_) {
      for (const auto& [operator_port, pipeline_port] : mappings) {
        auto key_it = collector_keys_.find({op_id, operator_port});
        if (key_it == collector_keys_.end()) continue;
        auto col_it = output_collectors_.find(key_it->second);
        if (col_it == output_collectors_.end()) continue;
        output_mapping_cache_.push_back({col_it->second.get(), pipeline_port});
      }
    }
  }

  std::vector<std::string> input_port_types_;
  std::vector<std::string> output_port_types_;
  std::vector<double> segment_bytecode_;
  std::vector<double> segment_constants_;
  std::vector<CompositeConnection> connections_;
  std::map<std::string, std::shared_ptr<Operator>> operators_;
  std::shared_ptr<Operator> entry_operator_;
  std::map<std::string, std::vector<std::pair<size_t, size_t>>> output_mappings_;
  std::map<std::string, std::shared_ptr<Collector>> output_collectors_;
  std::map<std::pair<std::string, size_t>, std::string> collector_keys_;
  std::vector<OutputMappingFlat> output_mapping_cache_;

  // Segment state
  bool has_key_{false};
  double current_key_{0.0};
  std::map<size_t, std::unique_ptr<BaseMessage>> last_output_buffer_;
};

}  // namespace rtbot

#endif  // PIPELINE_H
