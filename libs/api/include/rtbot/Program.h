#ifndef PROGRAM_H
#define PROGRAM_H

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include <variant>

#include "OperatorJson.h"
#include "Prototype.h"
#include "rtbot/Collector.h"
#include "rtbot/Logger.h"
#include "rtbot/OperatorJson.h"
#include "rtbot/PortType.h"
#include "rtbot/jsonschema.hpp"
#include "rtbot/compiled/jit/JitCache.h"
#include "rtbot/compiled/jit/JitCompiledProgram.h"
#include "rtbot/compiled/jit/JitCompiler.h"

namespace rtbot {

using json = nlohmann::json;

using namespace std;

class Program {
 public:
  // Constructor from JSON string
  explicit Program(const std::string& json_string) : program_json_(json_string) {
    auto j = json::parse(program_json_);

    // Resolve prototypes before validation
    PrototypeHandler::resolve_prototypes(j);

    // Update program_json_ with resolved version
    program_json_ = j.dump();

    // Continue with existing validation and initialization
    nlohmann::json_schema::json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);
    validator.set_root_schema(rtbot_schema);
    validator.validate(j);

    // Always build the interpreter graph first — this preserves all existing
    // behaviour (serialization, debug mode, multi-port receive_batch, etc.).
    init_from_json();

    // Then attempt JIT compilation as an acceleration layer for the hot path
    // receive(Message<NumberData>). The probe runs once per process and caches
    // the result; if it fails, all subsequent Program constructions skip JIT
    // entirely. On any per-JSON failure we silently fall back to the interpreter.
    //
    // Force-disable knob: setting the environment variable RTBOT_DISABLE_JIT to
    // any non-empty value skips the JIT path entirely. Useful for A/B benchmarks
    // and for diagnosing JIT-vs-interpreter parity issues.
    //
    // Runtime override: set_force_interpreter_for_testing(true) flips the same
    // switch at runtime. The env var is captured once via a function-local static
    // and is read on every construction; the runtime flag is read on every
    // construction so a single process can build interpreter-mode and JIT-mode
    // Programs back-to-back for parity testing.
    static const bool jit_disabled_by_env_ = [] {
      const char* v = std::getenv("RTBOT_DISABLE_JIT");
      return v != nullptr && v[0] != '\0';
    }();
    const bool jit_disabled = jit_disabled_by_env_ || force_interpreter_flag_();
    if (!jit_disabled && rtbot::jit::probe_runtime_support()) {
      try {
        jit_program_ = rtbot::jit::JitCache::instance().get_or_compile(program_json_);
        parse_jit_output_mapping_(j);
        using_jit_ = true;
        // Cache the hot-path fields so Program::send can call segment_fn_
        // directly without dereferencing the unique_ptr on every tick.
        cached_jit_fn_           = jit_program_->raw_segment_fn();
        cached_jit_fn_vec_       = jit_program_->input_is_vector()
                                       ? jit_program_->raw_fn_vec()
                                       : nullptr;
        cached_jit_input_lane_width_ = jit_program_->input_lane_width();
        cached_jit_state_        = jit_program_->raw_state_buffer();
        cached_jit_out_t_buf_    = jit_program_->raw_out_t_buf();
        cached_jit_out_v_buf_    = jit_program_->raw_out_v_buf();
        cached_jit_out_port_id_buf_ = jit_program_->raw_out_port_id_buf();
        cached_jit_num_outputs_  = jit_program_->num_outputs();
      } catch (const std::exception&) {
        jit_program_.reset();
        cached_jit_fn_    = nullptr;
        cached_jit_fn_vec_ = nullptr;
        using_jit_ = false;
      }
    }
  }

  // Internal/debug: returns true when the JIT backend is active for this program.
  bool using_jit() const noexcept { return using_jit_; }

  // Runtime override that mirrors RTBOT_DISABLE_JIT but can be flipped at any
  // time. When set to true, every Program constructed afterwards will skip the
  // JIT path and use the interpreter, regardless of env-var state. Intended for
  // cross-mode parity tests that need to build both modes within one process.
  // Not thread-safe; toggle from one thread only.
  static void set_force_interpreter_for_testing(bool on) noexcept {
    force_interpreter_flag_() = on;
  }

  // Direct access to the JIT-compiled program when JIT is active. Returns
  // nullptr when this Program is using the FE interpreter (either because
  // JIT was unavailable, the JSON used unsupported opcodes, or restore was
  // called).
  //
  // For C++ callers in performance-critical hot loops who need every cycle:
  // grab this accessor and call JitCompiledProgram::receive(t, v) /
  // collect_outputs() directly. Bypasses the Program::send overhead (~15%
  // throughput improvement on Bollinger). Outputs are flat — caller is
  // responsible for the port-name mapping that Program::drain_outputs would
  // otherwise apply.
  //
  // The pointer is owned by Program; do NOT delete or hold beyond Program's
  // lifetime.
  rtbot::jit::JitCompiledProgram* jit_program() noexcept { return jit_program_.get(); }
  const rtbot::jit::JitCompiledProgram* jit_program() const noexcept { return jit_program_.get(); }

  string serialize_data() {
    if (using_jit_) {
      throw std::runtime_error(
          "serialize_data: not supported while JIT is active; "
          "call restore_data_from_json to switch to interpreter mode first");
    }
    json result;
    for (auto& [name, op] : operators_) {
      result[name] = op->collect();
    }
    return result.dump();
  }

  void restore_data_from_json(const string& json_state) {
    auto j = json::parse(json_state);
    for (auto& [name, op] : operators_) {
      op->restore_data_from_json(j.at(name));
    }
    // After restoring interpreter state, the JIT's internal state no longer
    // matches. Disable JIT so subsequent receives use the interpreter, which
    // now has the correctly restored state.
    if (using_jit_) {
      using_jit_      = false;
      cached_jit_fn_  = nullptr;
      cached_jit_fn_vec_ = nullptr;
      jit_program_.reset();
    }
  }

  // Message processing
  ProgramMsgBatch receive(std::unique_ptr<BaseMessage> msg, const std::string& port_id = "i1") {
    if (using_jit_) {
      if (jit_program_->input_is_vector()) {
        if (auto* vm = dynamic_cast<Message<VectorNumberData>*>(msg.get())) {
          const auto& values = *vm->data.values;
          if (values.size() >= jit_program_->input_lane_width()) {
            jit_program_->receive_vector(vm->time, values.data(),
                                          jit_program_->input_lane_width());
            return translate_jit_to_batch_(jit_program_->collect_outputs());
          }
        }
      } else {
        if (auto* nm = dynamic_cast<Message<NumberData>*>(msg.get())) {
          jit_program_->receive(nm->time, nm->data.value);
          return translate_jit_to_batch_(jit_program_->collect_outputs());
        }
      }
    }
    send_to_entry(std::move(msg), port_id, false);
    return collect_outputs(false);
  }

  ProgramMsgBatch receive(const Message<NumberData>& msg, const std::string& port_id = "i1") {
    if (using_jit_) {
      // JIT-only hot path: advance the JIT pipeline and return its output.
      // The interpreter graph is NOT advanced; it is kept only for fallback
      // construction, restore_data_from_json, and debug paths.
      jit_program_->receive(msg.time, msg.data.value);
      return translate_jit_to_batch_(jit_program_->collect_outputs());
    }
    return receive(create_message<NumberData>(msg.time, msg.data), port_id);
  }

  // Streaming hot-path API. send() advances the program state by one tick
  // without constructing or returning a batch. Callers drain accumulated
  // outputs periodically via drain_outputs(). For high-throughput pipelines
  // (especially JIT-compiled), this avoids the per-call batch allocation
  // overhead that receive(Message) imposes.
  //
  // Both JIT and interpreter paths are supported. On the interpreter path,
  // outputs accumulate in the internal sink queues until drain_outputs() is
  // called; no per-call batch is constructed.
  //
  // Two overloads:
  //   send(t, v)          — JIT hot path (port_id irrelevant; no std::string
  //                         construction whatsoever on this overload).
  //   send(t, v, port_id) — interpreter path or multi-port callers.
  //
  // The two-arg overload is kept separate so the compiler never materialises a
  // temporary std::string on the JIT hot path — even with LTO a default-arg
  // reference requires the string to be constructed before the branch.
  // always_inline forces inlining even when the compiler's size heuristics
  // would normally outline it. Without this the compiler emits a real
  // bl/ret pair, paying ~5-7 cycles of call overhead on every tick.
  __attribute__((always_inline)) void send(std::int64_t t, double v) {
    if (cached_jit_fn_ != nullptr) {
      // Direct call via cached function pointer — no unique_ptr dereference,
      // no JitCompiledProgram object load on the hot path.
      std::int32_t count = cached_jit_fn_(cached_jit_state_, t, v,
                                          cached_jit_out_t_buf_,
                                          cached_jit_out_v_buf_,
                                          cached_jit_out_port_id_buf_);
      if (count > 0) {
        // Emission is rare (windowed pipelines); indirection through jit_program_
        // here is amortized over many no-emit ticks and is not on the hot path.
        jit_program_->push_emissions(count, cached_jit_out_t_buf_,
                                     cached_jit_out_port_id_buf_,
                                     cached_jit_out_v_buf_,
                                     cached_jit_num_outputs_);
      }
      return;
    }
    static const std::string default_port{"i1"};
    send_interpreter_(t, v, default_port);
  }

  __attribute__((always_inline)) void send(std::int64_t t, double v, const std::string& port_id) {
    if (cached_jit_fn_ != nullptr) {
      std::int32_t count = cached_jit_fn_(cached_jit_state_, t, v,
                                          cached_jit_out_t_buf_,
                                          cached_jit_out_v_buf_,
                                          cached_jit_out_port_id_buf_);
      if (count > 0) {
        jit_program_->push_emissions(count, cached_jit_out_t_buf_,
                                     cached_jit_out_port_id_buf_,
                                     cached_jit_out_v_buf_,
                                     cached_jit_num_outputs_);
      }
      return;
    }
    send_interpreter_(t, v, port_id);
  }

  // Vector-input streaming hot-path API. Mirrors send(t, v) but takes a
  // pointer to `n` consecutive lane values. JIT path requires the program to
  // have been compiled with a vector input op whose lane width matches `n`;
  // otherwise the call falls through to the interpreter path which packs the
  // values into a Message<VectorNumberData> and dispatches via send_to_entry.
  //
  // The JIT fast path performs no heap allocation: it calls the cached vector
  // segment function pointer directly and pushes any emissions into the JIT
  // emit buffer for later draining via drain_outputs / drain_into.
  __attribute__((always_inline)) void send_vector(std::int64_t t,
                                                   const double* v,
                                                   std::size_t n) {
    if (cached_jit_fn_vec_ != nullptr) {
      std::int32_t count = cached_jit_fn_vec_(cached_jit_state_, t, v,
                                               cached_jit_out_t_buf_,
                                               cached_jit_out_v_buf_,
                                               cached_jit_out_port_id_buf_);
      if (count > 0) {
        jit_program_->push_emissions(count, cached_jit_out_t_buf_,
                                     cached_jit_out_port_id_buf_,
                                     cached_jit_out_v_buf_,
                                     cached_jit_num_outputs_);
      }
      return;
    }
    static const std::string default_port{"i1"};
    send_vector_interpreter_(t, v, n, default_port);
  }

  __attribute__((always_inline)) void send_vector(std::int64_t t,
                                                   const double* v,
                                                   std::size_t n,
                                                   const std::string& port_id) {
    if (cached_jit_fn_vec_ != nullptr) {
      std::int32_t count = cached_jit_fn_vec_(cached_jit_state_, t, v,
                                               cached_jit_out_t_buf_,
                                               cached_jit_out_v_buf_,
                                               cached_jit_out_port_id_buf_);
      if (count > 0) {
        jit_program_->push_emissions(count, cached_jit_out_t_buf_,
                                     cached_jit_out_port_id_buf_,
                                     cached_jit_out_v_buf_,
                                     cached_jit_num_outputs_);
      }
      return;
    }
    send_vector_interpreter_(t, v, n, port_id);
  }

  // Drain all outputs accumulated since the last drain (or since construction).
  // Returns them in the same ProgramMsgBatch shape as receive(). Cheap to call
  // if no outputs are pending. Empties the internal buffer.
  ProgramMsgBatch drain_outputs() {
    if (using_jit_) {
      return translate_jit_to_batch_(jit_program_->collect_outputs());
    }
    return collect_outputs(false);
  }

  // Callback-based drain that bypasses ProgramMsgBatch construction entirely.
  // For each accumulated emission, invokes cb(time, values_ptr, num_values).
  // On the JIT path this avoids all heap allocation and map insertions —
  // the callback receives a raw pointer into the EmittedRecord's value storage.
  // On the interpreter path this falls back to drain_outputs() and iterates the
  // resulting batch, grouping by timestamp so the callback shape matches the
  // JIT contract: one invocation per emitted tick, all output port values
  // bundled together in the same flat-slot order the JIT writes.
  //
  // Signature: void(int64_t time, const double* values, std::size_t num_values)
  template <class Callback>
  inline void drain_into(Callback&& cb) {
    if (using_jit_) {
      // Single-terminal: dump the whole record (preserves the existing
      // contract for downstream consumers like the parity helper).
      // Multi-terminal session shape: split the flat record per terminal so
      // the callback shape matches the interpreter's per-op walk below.
      const std::size_t terminals = [&]() {
        std::set<std::string> ids;
        for (const auto& jop : jit_output_ports_) ids.insert(jop.op_id);
        return ids.size();
      }();
      if (terminals <= 1) {
        for (const auto& r : jit_program_->collect_outputs()) {
          cb(r.time, r.values.data(), r.values.size());
        }
        return;
      }
      // Group jit_output_ports_ entries by op_id, preserving enumeration
      // order. For each emitted record, emit one cb per terminal whose flat
      // slot range falls within the record.
      auto records = jit_program_->collect_outputs();
      if (records.empty()) return;

      // Patch vector lane widths from the first record (mirrors the same
      // logic translate_jit_to_batch_ uses). Total slots = sum of widths;
      // scalar entries take 1 slot each, vector entries split the residual
      // evenly across vector entries.
      {
        std::size_t fixed = 0;
        std::size_t vector_count = 0;
        for (const auto& jop : jit_output_ports_) {
          if (jop.is_vector) ++vector_count;
          else               ++fixed;
        }
        if (vector_count > 0) {
          const std::size_t remaining =
              (records.front().values.size() > fixed)
                  ? records.front().values.size() - fixed
                  : 0;
          const std::size_t per_vec_w =
              (vector_count > 0 && remaining > 0)
                  ? remaining / vector_count
                  : 1;
          for (auto& jop : jit_output_ports_) {
            if (jop.is_vector && jop.lane_width <= 1) {
              jop.lane_width = per_vec_w == 0 ? 1 : per_vec_w;
            }
          }
          // Recompute slot bases assuming session-shape multi-terminal
          // layout: cumulative running base over enumeration order.
          std::size_t cum = 0;
          for (auto& jop : jit_output_ports_) {
            jop.flat_slot_base = cum;
            cum += jop.is_vector ? jop.lane_width : 1;
          }
        }
      }

      std::map<std::string, std::vector<std::size_t>>
          term_entry_indices;  // op_id -> indices into jit_output_ports_
      for (std::size_t i = 0; i < jit_output_ports_.size(); ++i) {
        term_entry_indices[jit_output_ports_[i].op_id].push_back(i);
      }
      // Group emissions per terminal then walk in (op_id, time) order so the
      // callback shape matches the interpreter's per-op-per-time walk below.
      std::map<std::string, std::vector<std::pair<std::int64_t,
                                                    std::vector<double>>>>
          per_term;
      for (const auto& r : records) {
        for (const auto& [op_id, idxs] : term_entry_indices) {
          std::size_t lo = std::numeric_limits<std::size_t>::max();
          std::size_t hi = 0;
          for (std::size_t i : idxs) {
            const auto& jop = jit_output_ports_[i];
            const std::size_t w = jop.is_vector ? jop.lane_width : 1;
            if (jop.flat_slot_base < lo) lo = jop.flat_slot_base;
            if (jop.flat_slot_base + w > hi) hi = jop.flat_slot_base + w;
          }
          if (lo == std::numeric_limits<std::size_t>::max()) continue;
          if (hi > r.values.size()) hi = r.values.size();
          if (lo >= hi) continue;
          per_term[op_id].emplace_back(
              r.time,
              std::vector<double>(r.values.begin() + lo,
                                   r.values.begin() + hi));
        }
      }
      for (const auto& [op_id, emits] : per_term) {
        for (const auto& [t, vals] : emits) {
          cb(t, vals.data(), vals.size());
        }
      }
      return;
    }
    auto batch = collect_outputs(false);
    if (batch.empty()) return;

    // For each output op, compute the flat-slot layout matching the JIT's
    // output_flat_slot_base convention: ports are laid out in the Output op's
    // input-port index order (port 0 first, then port 1, ...). Each port
    // contributes output_port_width(p) consecutive slots. Vector wires fan out
    // across consecutive lanes.
    //
    // batch is unordered_map; sort keys so the callback sequence is stable
    // and matches the JIT path's std::map walk above.
    std::vector<std::string> sorted_op_ids;
    sorted_op_ids.reserve(batch.size());
    for (const auto& kv : batch) sorted_op_ids.push_back(kv.first);
    std::sort(sorted_op_ids.begin(), sorted_op_ids.end());
    for (const auto& op_id : sorted_op_ids) {
      const auto& op_batch = batch.at(op_id);
      auto op_it = operators_.find(op_id);
      if (op_it == operators_.end()) continue;
      const auto& op = op_it->second;
      const std::size_t n_ports = op->num_output_ports();

      // Build slot base table indexed by port index. total_slots is the sum
      // of all port widths.
      std::vector<std::size_t> slot_base(n_ports, 0);
      std::vector<std::size_t> slot_width(n_ports, 1);
      std::size_t total_slots = 0;
      for (std::size_t p = 0; p < n_ports; ++p) {
        slot_base[p] = total_slots;
        const auto pt = op->get_output_port_type(p);
        const bool is_vec =
            (PortType::type_index_to_string(pt) == "vector_number");
        // Width is determined per emission for vectors (data-dependent).
        // Default to 1; override below when we see the actual message.
        slot_width[p] = is_vec ? 0 : 1;
        total_slots += is_vec ? 0 : 1;
      }

      // For pure scalar output ops, total_slots is final. For vector ports we
      // need to defer to emission inspection to learn the lane width. Walk
      // once to discover any vector port widths and rebuild bases.
      bool has_vector = false;
      for (std::size_t p = 0; p < n_ports; ++p) {
        if (slot_width[p] == 0) { has_vector = true; break; }
      }
      if (has_vector) {
        for (const auto& [port_name, port_msgs] : op_batch) {
          if (port_msgs.empty()) continue;
          const std::size_t p =
              OperatorJson::parse_port_name(port_name).index;
          if (p >= n_ports) continue;
          if (slot_width[p] != 0) continue;
          if (const auto* vm = dynamic_cast<const Message<VectorNumberData>*>(
                  port_msgs.front().get())) {
            slot_width[p] = vm->data.values->size();
          } else {
            slot_width[p] = 1;
          }
        }
        total_slots = 0;
        for (std::size_t p = 0; p < n_ports; ++p) {
          slot_base[p] = total_slots;
          if (slot_width[p] == 0) slot_width[p] = 1;
          total_slots += slot_width[p];
        }
      }

      if (total_slots == 0) continue;

      // Group emissions by timestamp. Use a map<int64_t, vector<double>> so
      // timestamps come out in monotonic order. Slots are pre-filled with NaN
      // so a port that did not emit at a given t is unambiguously absent.
      std::map<std::int64_t, std::vector<double>> by_time;
      const double nan_v = std::numeric_limits<double>::quiet_NaN();

      for (const auto& [port_name, port_msgs] : op_batch) {
        const std::size_t p =
            OperatorJson::parse_port_name(port_name).index;
        if (p >= n_ports) continue;
        const std::size_t base = slot_base[p];
        const std::size_t width = slot_width[p];
        for (const auto& msg_ptr : port_msgs) {
          if (const auto* nm =
                  dynamic_cast<const Message<NumberData>*>(msg_ptr.get())) {
            auto& vals = by_time[nm->time];
            if (vals.empty()) vals.assign(total_slots, nan_v);
            if (base < total_slots) vals[base] = nm->data.value;
            continue;
          }
          if (const auto* vm =
                  dynamic_cast<const Message<VectorNumberData>*>(
                      msg_ptr.get())) {
            auto& vals = by_time[vm->time];
            if (vals.empty()) vals.assign(total_slots, nan_v);
            const auto& src = *vm->data.values;
            const std::size_t n_copy =
                std::min<std::size_t>(width, src.size());
            for (std::size_t k = 0; k < n_copy; ++k) {
              if (base + k < total_slots) vals[base + k] = src[k];
            }
            continue;
          }
        }
      }

      for (const auto& [t, vals] : by_time) {
        cb(t, vals.data(), vals.size());
      }
    }
  }

  // Zero-allocation, per-terminal drain. Each accumulated emission is split
  // per declared output port and dispatched as one callback invocation:
  //   cb(int64_t time, const std::string& op_id, const double* values,
  //      std::size_t n_values)
  // For scalar ports n_values == 1; for vector ports n_values == lane_width.
  // The values pointer is valid only for the duration of the callback.
  //
  // Mirrors translate_jit_to_batch_'s three shapes (per-port Demux,
  // single-vector-terminal, multi-port session) but skips the
  // ProgramMsgBatch / Message<...> construction. On the interpreter path
  // falls back to draining the sink and walking the resulting batch.
  //
  // Intended for binding layers that pack outputs into a flat binary frame
  // (Java direct ByteBuffer, etc) where the round-trip through a typed
  // batch dominates the per-emission cost.
  template <class Callback>
  inline void drain_records(Callback&& cb, bool consume = true) {
    if (using_jit_) {
      const bool per_port = (jit_program_->max_emits_per_call() > 1);
      const bool single_terminal_single_vector =
          (jit_output_ports_.size() == 1 && jit_output_ports_[0].is_vector);

      bool widths_patched = false;
      auto patch_widths = [&](std::size_t rec_n) {
        std::size_t fixed = 0;
        std::size_t vector_count = 0;
        for (const auto& jop : jit_output_ports_) {
          if (jop.is_vector) ++vector_count;
          else               ++fixed;
        }
        if (vector_count == 0) return;
        const std::size_t remaining = (rec_n > fixed) ? rec_n - fixed : 0;
        const std::size_t per_vec_w =
            (vector_count > 0 && remaining > 0) ? remaining / vector_count : 1;
        for (auto& jop : jit_output_ports_) {
          if (jop.is_vector && jop.lane_width <= 1) {
            jop.lane_width = per_vec_w == 0 ? 1 : per_vec_w;
          }
        }
        const std::size_t terminals = [&]() {
          std::set<std::string> ids;
          for (const auto& jop : jit_output_ports_) ids.insert(jop.op_id);
          return ids.size();
        }();
        if (terminals > 1) {
          std::size_t cum = 0;
          for (auto& jop : jit_output_ports_) {
            jop.flat_slot_base = cum;
            cum += jop.is_vector ? jop.lane_width : 1;
          }
        }
      };

      jit_program_->drain_records_raw(
          [&](std::int64_t time, std::int32_t port_id, const double* values,
              std::size_t n_values) {
            if (!widths_patched) {
              patch_widths(n_values);
              widths_patched = true;
            }
            if (per_port) {
              const std::size_t pid =
                  (port_id >= 0 && static_cast<std::size_t>(port_id) <
                                       jit_output_ports_.size())
                      ? static_cast<std::size_t>(port_id)
                      : 0;
              const std::string& op_id = (pid < jit_output_ports_.size())
                                              ? jit_output_ports_[pid].op_id
                                              : jit_output_op_id_;
              const double v = (n_values == 0) ? 0.0 : values[0];
              cb(time, op_id, &v, std::size_t{1});
              return;
            }
            if (single_terminal_single_vector) {
              const auto& port = jit_output_ports_[0];
              cb(time, port.op_id, values, n_values);
              return;
            }
            for (const auto& port : jit_output_ports_) {
              const std::size_t base = port.flat_slot_base;
              if (base >= n_values) continue;
              if (!port.is_vector) {
                cb(time, port.op_id, values + base, std::size_t{1});
              } else {
                const std::size_t end =
                    std::min(base + port.lane_width, n_values);
                cb(time, port.op_id, values + base, end - base);
              }
            }
          },
          consume);
      return;
    }

    // Interpreter fallback: drain the batch and dispatch per-message. Order
    // matches the existing decode_program_batch shape (one callback per
    // Message<NumberData> / Message<VectorNumberData>) so binding-layer
    // consumers see identical semantics on both paths.
    auto batch = collect_outputs(false);
    for (const auto& [op_id, op_batch] : batch) {
      for (const auto& [port_name, messages] : op_batch) {
        (void)port_name;
        for (const auto& message : messages) {
          if (auto* vm = dynamic_cast<const Message<VectorNumberData>*>(
                  message.get())) {
            const auto& vals = vm->data.values ? *vm->data.values
                                                : std::vector<double>{};
            cb(static_cast<std::int64_t>(vm->time), op_id, vals.data(),
               vals.size());
            continue;
          }
          if (auto* nm = dynamic_cast<const Message<NumberData>*>(
                  message.get())) {
            const double v = nm->data.value;
            cb(static_cast<std::int64_t>(nm->time), op_id, &v,
               std::size_t{1});
          }
        }
      }
    }
  }

  ProgramMsgBatch receive_debug(std::unique_ptr<BaseMessage> msg, const std::string& port_id = "i1") {
    for (auto& [_, op] : operators_) {
      op->clear_debug_output_queues();
    }
    send_to_entry(std::move(msg), port_id, true);
    return collect_outputs(true);
  }

  ProgramMsgBatch receive_debug(const Message<NumberData>& msg, const std::string& port_id = "i1") {
    return receive_debug(create_message<NumberData>(msg.time, msg.data), port_id);
  }

  // Batch entry: push all messages from a multi-port buffer into the entry
  // operator's queues, then run a single execute() pass, then collect+clear
  // outputs once. Semantically equivalent to calling receive() once per
  // message in arrival order, but amortizes the per-message scheduling cost
  // (virtual dispatch, propagate_outputs recursion, collect/clear) over the
  // whole burst. Relies on the rtbot invariant that messages within a port
  // are monotone in time; sync_data_inputs is state-preserving regardless of
  // how many messages are queued on each port.
  ProgramMsgBatch receive_batch(
      const std::map<std::string, std::vector<std::unique_ptr<BaseMessage>>>& port_messages) {
    if (using_jit_ && batch_is_jit_compatible_(port_messages)) {
      // Walk every message in arrival order and dispatch through the JIT.
      // Single-port callers (the common case from Java/Ignition) hit the
      // straight-line scalar or vector dispatch with no extra bookkeeping.
      const bool input_vec = jit_program_->input_is_vector();
      for (const auto& [port_id, messages] : port_messages) {
        if (messages.empty()) continue;
        for (const auto& msg : messages) {
          if (input_vec) {
            auto* vm = static_cast<Message<VectorNumberData>*>(msg.get());
            const auto& values = *vm->data.values;
            jit_program_->receive_vector(vm->time, values.data(),
                                          cached_jit_input_lane_width_);
          } else {
            auto* nm = static_cast<Message<NumberData>*>(msg.get());
            jit_program_->receive(nm->time, nm->data.value);
          }
        }
      }
      return translate_jit_to_batch_(jit_program_->collect_outputs());
    }
    send_batch_to_entry(port_messages, false);
    return collect_outputs(false);
  }

  // Raw-buffer ingress. `data` is a row-major double buffer of `num_rows` ×
  // `num_cols`; `times[r]` is the monotone timestamp of row r. When the entry
  // operator overrides Operator::receive_data_buffer (e.g. BurstAggregate)
  // this skips Message allocation entirely and streams the buffer straight
  // into the operator's kernel. Operators that don't override fall back to
  // per-row Message creation (default Operator impl). Semantically equivalent
  // to receive_batch over a parallel vector of Message<VectorNumberData>.
  ProgramMsgBatch receive_buffer(const std::string& port_id,
                                   const double* data, size_t num_rows,
                                   size_t num_cols,
                                   const timestamp_t* times) {
    if (using_jit_ && cached_jit_fn_vec_ != nullptr &&
        num_cols >= cached_jit_input_lane_width_) {
      for (size_t r = 0; r < num_rows; ++r) {
        jit_program_->receive_vector(times[r], data + r * num_cols,
                                      cached_jit_input_lane_width_);
      }
      return translate_jit_to_batch_(jit_program_->collect_outputs());
    }
    if (using_jit_ && !jit_program_->input_is_vector() && num_cols == 1) {
      for (size_t r = 0; r < num_rows; ++r) {
        jit_program_->receive(times[r], data[r]);
      }
      return translate_jit_to_batch_(jit_program_->collect_outputs());
    }
    auto port_info = OperatorJson::parse_port_name(port_id);
    auto& entry = operators_[entry_operator_id_];
    entry->receive_data_buffer(data, num_rows, num_cols, times,
                                 port_info.index, /*debug=*/false);
    entry->execute(false);
    return collect_outputs(false);
  }

  ProgramMsgBatch receive_batch_debug(
      const std::map<std::string, std::vector<std::unique_ptr<BaseMessage>>>& port_messages) {
    for (auto& [_, op] : operators_) {
      op->clear_debug_output_queues();
    }
    send_batch_to_entry(port_messages, true);
    return collect_outputs(true);
  }

  // Getters
  const string& get_entry_operator() const { return entry_operator_id_; }
  const map<string, vector<size_t>>& get_output_mappings() const { return output_mappings_; }
  vector<string> entry_ports() const;

  // Static factory methods

 private:
  // Runtime force-interpreter flag. Function-local static returned by reference
  // so the toggle is visible across translation units without requiring a
  // dedicated .cpp file.
  static bool& force_interpreter_flag_() noexcept {
    static bool flag = false;
    return flag;
  }

  string program_json_;
  map<string, shared_ptr<Operator>> operators_;
  string entry_operator_id_;
  map<string, vector<size_t>> output_mappings_;
  // Program-owned multi-port sink. One Collector with one data port per
  // output in output_mappings_; emit_output pushes straight into the
  // matching port queue via the sink_queue fast path (no virtual
  // receive_data). Provenance (source operator + output port) is read from
  // each port's inbound connection ref — no separate index needed.
  std::shared_ptr<Collector> sink_;

  // JIT acceleration layer. jit_program_ owns the JitCompiledProgram; all cold
  // paths (drain, debug, serialization) go through it. The four fields below
  // are cached at construction so Program::send can call the JIT function
  // pointer directly without dereferencing the unique_ptr on the hot path.
  std::unique_ptr<rtbot::jit::JitCompiledProgram> jit_program_;
  bool using_jit_{false};

  // Cached hot-path fields. Valid iff using_jit_ == true; nullptr when inactive.
  // These point into jit_program_'s internal storage (state_, out_t_buf_,
  // out_v_buf_, out_port_id_buf_) which is stable for the lifetime of jit_program_.
  rtbot::jit::JitCompiledProgram::SegmentFnT cached_jit_fn_{nullptr};
  // Vector-input function pointer. Non-null iff using_jit_ == true AND the
  // program was compiled with a vector input op (input_is_vector() == true).
  rtbot::jit::JitCompiledProgram::SegmentFnVecT cached_jit_fn_vec_{nullptr};
  std::size_t   cached_jit_input_lane_width_{1};
  double*       cached_jit_state_{nullptr};
  std::int64_t* cached_jit_out_t_buf_{nullptr};
  double*       cached_jit_out_v_buf_{nullptr};
  std::int32_t* cached_jit_out_port_id_buf_{nullptr};
  std::size_t   cached_jit_num_outputs_{0};

  // Output op id from the JSON "output" field. Used by translate_jit_to_batch_
  // to key the returned ProgramMsgBatch correctly. When the JSON declares
  // multiple terminal ops via the top-level `output` map (rtbot-sql session
  // shape), this is the FIRST terminal id; per-terminal routing for the
  // remaining ports is driven by jit_output_ports_ entries.
  std::string jit_output_op_id_;
  // Per-output-port shape, captured at ctor time from the interpreter graph.
  // Drives translate_jit_to_batch_'s choice between scalar Message<NumberData>
  // (port type "number") and vector Message<VectorNumberData> (port type
  // "vector_number"). Indices match the JSON `output` array's flat enumeration
  // order across every terminal op (terminal_a's ports first, then terminal_b's,
  // etc.) — this is the same order used by the JIT's flat slot layout.
  struct JitOutputPort {
    std::string op_id;       // terminal op id this port belongs to
    std::string name;        // "o1", "o2", ...
    bool is_vector{false};
    std::size_t lane_width{1};
    // Flat slot base in the JIT's out_v_arr for this entry's first lane.
    // Computed at parse time so translate_jit_to_batch_ can dispatch directly
    // without re-walking width metadata per record.
    std::size_t flat_slot_base{0};
  };
  std::vector<JitOutputPort> jit_output_ports_;

  void init_from_json() {
    RTBOT_LOG_DEBUG("Initializing program from JSON");
    auto j = json::parse(program_json_);

    // First pass: Create all operators
    for (const json& op_json : j["operators"]) {
      create_operator("", op_json);
    }

    // Second pass: Create connections
    for (const json& conn : j["connections"]) {
      create_connection(conn);
    }

    entry_operator_id_ = j["entryOperator"];
    if (!operators_[entry_operator_id_]) {
      throw runtime_error("Entry operator not found: " + entry_operator_id_);
    }

    // Parse output mappings
    if (!j.contains("output")) {
      throw runtime_error("Program JSON must contain 'output' field");
    }

    for (auto it = j["output"].begin(); it != j["output"].end(); ++it) {
      string op_id = resolve_operator_id(it.key());  // Use resolve_operator_id here
      vector<size_t> ports;
      for (const auto& port : it.value()) {
        ports.push_back(OperatorJson::parse_port_name(port).index);
      }
      output_mappings_[op_id] = ports;
    }

    // Attach Program-owned sinks to output-mapped operators.
    setup_output_sinks_();
  }

  void setup_output_sinks_() {
    struct OutputBinding { std::shared_ptr<Operator> op; size_t port_idx; };
    std::vector<OutputBinding> bindings;
    std::vector<std::string> port_types;

    for (const auto& [op_id, ports] : output_mappings_) {
      auto op_it = operators_.find(op_id);
      if (op_it == operators_.end()) continue;
      for (size_t port_idx : ports) {
        port_types.push_back(
            PortType::type_index_to_string(op_it->second->get_output_port_type(port_idx)));
        bindings.push_back({op_it->second, port_idx});
      }
    }

    if (bindings.empty()) return;

    sink_ = make_collector("__program_sink__", port_types);
    for (size_t i = 0; i < bindings.size(); ++i) {
      bindings[i].op->connect(sink_, bindings[i].port_idx, /*child_port_index=*/i, PortKind::DATA);
    }
  }

  // Parse the "output" field from the JSON and record the per-terminal port
  // shape for use by translate_jit_to_batch_. Walks every entry of the
  // top-level `output` map (rtbot-sql session shape: multiple terminals fused
  // into one Program), populating jit_output_ports_ in the SAME enumeration
  // order the JIT's JsonParser uses when synthesizing the flat-slot layout
  // (map iteration order over terminals, then port_names within each terminal).
  // jit_output_op_id_ is set to the first terminal id for cases where callers
  // expect a single key.
  void parse_jit_output_mapping_(const json& j) {
    if (!j.contains("output") || j["output"].empty()) {
      throw std::runtime_error("Program: JIT path requires non-empty 'output' field");
    }

    jit_output_ports_.clear();
    bool first_seen = false;

    // The JIT's flat out_v_arr layout matches a single Output op's input-port
    // layout: slot k holds whichever connection has to_port=k, with vector
    // ports occupying width-N consecutive slots. For a physical (single-
    // terminal) Output op this is the actual op's port layout. For session
    // shape (multiple terminals fused via the top-level "output" map, no
    // physical Output node) the JIT synthesizes an Output op whose input
    // ports are populated from `output` in std::map iteration order — i.e.
    // exactly the order we encounter them here.
    //
    // We compute each entry's flat slot base as the cumulative width of all
    // previously enumerated entries. For synth mode this is exact (matches
    // JsonParser's synthesizer cursor). For the physical single-terminal
    // case we re-derive the same number by sorting port-name indices: when
    // the user lists ports in shuffled order ([o2, o1, o3]), each entry's
    // slot base is determined by counting widths of ports with smaller
    // port-name index.
    std::size_t terminal_count = 0;
    for (auto it = j["output"].begin(); it != j["output"].end(); ++it) {
      ++terminal_count;
    }
    const bool single_terminal = (terminal_count == 1);

    std::size_t cum_slot = 0;
    for (auto it = j["output"].begin(); it != j["output"].end(); ++it) {
      const std::string term_id = it.key();
      if (!first_seen) {
        jit_output_op_id_ = term_id;
        first_seen = true;
      }
      auto resolved_id = resolve_operator_id(term_id);
      auto op_it = operators_.find(resolved_id);

      // Capture this terminal's per-entry shape (port name + lane width)
      // and the parsed port index used as the slot key for single-terminal
      // graphs.
      struct Entry {
        std::string name;
        std::size_t port_index{0};
        bool is_vector{false};
        std::size_t lane_width{1};
      };
      std::vector<Entry> entries;
      for (const auto& port_name : it.value()) {
        Entry e;
        e.name = port_name.get<std::string>();
        e.port_index = OperatorJson::parse_port_name(e.name).index;
        if (op_it != operators_.end()) {
          const auto pt = op_it->second->get_output_port_type(e.port_index);
          if (PortType::type_index_to_string(pt) == "vector_number") {
            e.is_vector = true;
            // For VectorCompose the lane width equals the number of input ports
            // and is always known statically — read it directly so that
            // heterogeneous-width multi-terminal session shapes compute correct
            // per-terminal flat-slot bases without lazy per-record resolution.
            auto* vc = dynamic_cast<VectorCompose*>(op_it->second.get());
            e.lane_width = vc ? vc->get_num_ports() : 0;
          } else {
            e.lane_width = 1;
          }
        }
        entries.push_back(std::move(e));
      }

      if (single_terminal) {
        // For physical single-terminal Output: the slot for port "oN" is the
        // sum of widths of all input ports with index < N. When the operator
        // exposes that port count (n_ports) we can compute this exactly for
        // scalar layouts. Vector lane widths are filled in lazily at first
        // emission via the resolve_widths step in translate_jit_to_batch_.
        // For homogeneous scalar Output ops (the common case) we compute it
        // here directly by parsed port index.
        for (auto& e : entries) {
          JitOutputPort jop;
          jop.op_id = term_id;
          jop.name = std::move(e.name);
          jop.is_vector = e.is_vector;
          jop.lane_width = e.lane_width == 0 ? 1 : e.lane_width;
          // Slot base = port_index for all-scalar layouts. Vector layouts
          // resolve at first record (see translate_jit_to_batch_).
          jop.flat_slot_base = e.port_index;
          jit_output_ports_.push_back(std::move(jop));
        }
      } else {
        // Session shape: the synth Output assigns one input port per entry
        // in (terminal, port_name) iteration order. cum_slot tracks the
        // running base. Vector widths are still resolved lazily; for the
        // initial mapping we treat unknown widths as 1 (cum_slot is then
        // patched at first emission for vector ports).
        for (auto& e : entries) {
          JitOutputPort jop;
          jop.op_id = term_id;
          jop.name = std::move(e.name);
          jop.is_vector = e.is_vector;
          jop.lane_width = e.lane_width == 0 ? 1 : e.lane_width;
          jop.flat_slot_base = cum_slot;
          cum_slot += jop.lane_width;  // lane_width is 1 for scalar, N for VectorCompose
          jit_output_ports_.push_back(std::move(jop));
        }
      }
    }
  }

  // Translate the flat EmittedRecord vector produced by JitCompiledProgram into
  // the ProgramMsgBatch shape expected by callers.
  //
  // Three record shapes are supported:
  //   1. Vector port (single-emit, output port declared "vector_number"): all
  //      record values are bundled into a single Message<VectorNumberData> on
  //      the lone declared output port.
  //   2. Per-port (multi-emit programs, e.g. Demultiplexer): each record
  //      carries a single scalar at values[0] targeting port "o{port_id+1}".
  //   3. Scalar broadcast: each record carries num_outputs scalars; values[k]
  //      lands on port "o{k+1}". Used by classic single-emit pipelines.
  ProgramMsgBatch translate_jit_to_batch_(
      std::vector<rtbot::jit::EmittedRecord> records) {
    ProgramMsgBatch batch;
    if (records.empty()) return batch;

    const bool per_port = (jit_program_->max_emits_per_call() > 1);
    const bool single_terminal_single_vector =
        (jit_output_ports_.size() == 1 && jit_output_ports_[0].is_vector);

    // Resolve vector lane widths from the first record's values vector. The
    // JIT writes total_slots = sum_of_widths doubles per record. When more
    // than one vector port exists we attribute scalar slots first, then
    // split the remainder evenly across the vector ports. For the common
    // single-vector-port-among-scalars case this is exact; for multi-vector
    // mixes (rare in practice) the widths are inferred from the record size
    // — vector terminals in session shape stamp port_widths during JSON
    // parse so the layout matches the JIT's emit.
    auto patch_vector_widths = [&](const rtbot::jit::EmittedRecord& rec) {
      std::size_t fixed = 0;
      std::size_t vector_count = 0;
      for (const auto& jop : jit_output_ports_) {
        if (jop.is_vector) ++vector_count;
        else               ++fixed;
      }
      if (vector_count == 0) return;
      const std::size_t remaining =
          (rec.values.size() > fixed) ? rec.values.size() - fixed : 0;
      const std::size_t per_vec_w =
          (vector_count > 0 && remaining > 0)
              ? remaining / vector_count
              : 1;
      // Patch lane widths for vector entries that don't already have one.
      // Then re-walk slot bases in (op_id, port_index)-stable order so
      // downstream slot lookups land on the right doubles.
      for (auto& jop : jit_output_ports_) {
        if (jop.is_vector && jop.lane_width <= 1) {
          jop.lane_width = per_vec_w == 0 ? 1 : per_vec_w;
        }
      }
      // Recompute slot bases for session-shape multi-port layouts where the
      // initial pass treated vector ports as zero-width. Single-terminal
      // physical Output layouts use parsed port-name indices and don't need
      // a rebuild here.
      const std::size_t terminals = [&]() {
        std::set<std::string> ids;
        for (const auto& jop : jit_output_ports_) ids.insert(jop.op_id);
        return ids.size();
      }();
      if (terminals > 1) {
        std::size_t cum = 0;
        for (auto& jop : jit_output_ports_) {
          jop.flat_slot_base = cum;
          cum += jop.is_vector ? jop.lane_width : 1;
        }
      }
    };

    bool widths_patched = false;

    for (auto& rec : records) {
      if (!widths_patched) {
        patch_vector_widths(rec);
        widths_patched = true;
      }

      // Multi-emit path: the JIT-encoded port_id selects exactly one output
      // port. Today only Demux/Mux/TopK paths take this branch, and those
      // remain single-terminal — keep the historical behaviour but route
      // through jit_output_ports_ so callers always see the declared
      // terminal id (not jit_output_op_id_) when port_id is in range.
      if (per_port) {
        const std::size_t pid =
            (rec.port_id >= 0 && static_cast<std::size_t>(rec.port_id) <
                                     jit_output_ports_.size())
                ? static_cast<std::size_t>(rec.port_id)
                : 0;
        // Demux/Mux: rec.values[0] holds the scalar lane value. Use the
        // entry's listed name — for the historical layout where the user
        // does not list every port, fall back to "o{port_id+1}" so the
        // legacy contract is preserved.
        std::string port_name = (pid < jit_output_ports_.size())
                                    ? jit_output_ports_[pid].name
                                    : ("o" + std::to_string(rec.port_id + 1));
        const std::string op_id = (pid < jit_output_ports_.size())
                                      ? jit_output_ports_[pid].op_id
                                      : jit_output_op_id_;
        const double v = rec.values.empty() ? 0.0 : rec.values[0];
        batch[op_id][port_name].push_back(
            create_message<NumberData>(rec.time, NumberData{v}));
        continue;
      }

      // Single-emit, single vector terminal: bundle the whole record into one
      // VectorNumberData. Preserves the historical fast path for KeyedPipeline
      // / FusedExpressionVector / BurstAggregate single-Output programs.
      if (single_terminal_single_vector) {
        std::vector<double> vals = rec.values;
        const auto& port = jit_output_ports_[0];
        batch[port.op_id][port.name].push_back(
            create_message<VectorNumberData>(
                rec.time, VectorNumberData{std::move(vals)}));
        continue;
      }

      // Single-emit, multi-port (possibly multi-terminal session shape):
      // dispatch each declared entry to its flat slot range. Scalar ports
      // pull one double from rec.values[base]; vector ports pull lane_width
      // consecutive doubles.
      for (const auto& port : jit_output_ports_) {
        const std::size_t base = port.flat_slot_base;
        if (base >= rec.values.size()) continue;
        if (!port.is_vector) {
          batch[port.op_id][port.name].push_back(
              create_message<NumberData>(rec.time,
                                          NumberData{rec.values[base]}));
        } else {
          const std::size_t end =
              std::min(base + port.lane_width, rec.values.size());
          std::vector<double> lane(rec.values.begin() + base,
                                    rec.values.begin() + end);
          batch[port.op_id][port.name].push_back(
              create_message<VectorNumberData>(
                  rec.time, VectorNumberData{std::move(lane)}));
        }
      }
    }
    return batch;
  }

  void create_operator(const std::string& parent_prefix, const json& op_json) {
    string local_id = op_json["id"];
    string qualified_id = parent_prefix.empty() ? local_id : parent_prefix + "::" + local_id;

    auto op = OperatorJson::read_op(op_json.dump());
    operators_[qualified_id] = op;

    // If this is a Pipeline, recursively create its internal operators
    if (auto pipeline = std::dynamic_pointer_cast<Pipeline>(op)) {
      auto pipeline_json = json::parse(op_json.dump());
      if (pipeline_json.contains("operators")) {
        for (const auto& internal_op : pipeline_json["operators"]) {
          create_operator(qualified_id, internal_op);
        }
      }
    }

    // If this is a TriggerSet, recursively create its internal operators
    if (auto trigger_set = std::dynamic_pointer_cast<TriggerSet>(op)) {
      auto ts_json = json::parse(op_json.dump());
      if (ts_json.contains("operators")) {
        for (const auto& internal_op : ts_json["operators"]) {
          create_operator(qualified_id, internal_op);
        }
      }
    }
  }

  void create_connection(const json& conn) {
    string from_qual_id = resolve_operator_id(conn["from"]);
    string to_qual_id = resolve_operator_id(conn["to"]);

    auto from_port = OperatorJson::parse_port_name(conn.value("fromPort", "o1"));
    auto to_port = OperatorJson::parse_port_name(conn.value("toPort", "i1"));

    if (!operators_[from_qual_id] || !operators_[to_qual_id]) {
      throw runtime_error("Invalid operator reference in connection from " + from_qual_id + " to " + to_qual_id);
    }

    operators_[from_qual_id]->connect(operators_[to_qual_id], from_port.index, to_port.index, to_port.kind);
  }

  string resolve_operator_id(const string& id) {
    // First check if it's already a qualified ID
    if (operators_.find(id) != operators_.end()) {
      return id;
    }

    // Search through all qualified IDs for a match
    string suffix = "::" + id;
    for (const auto& [qual_id, _] : operators_) {
      if ((qual_id.length() >= suffix.length() &&
           qual_id.compare(qual_id.length() - suffix.length(), suffix.length(), suffix) == 0) ||
          qual_id == id) {
        return qual_id;
      }
    }

    throw runtime_error("Could not resolve operator ID: " + id);
  }

  // Out-of-line interpreter path for send(). Kept out of the header to avoid
  // pulling internal types (OperatorJson, create_message, etc.) into callers
  // that only use the JIT path. Implemented in bindings.cpp or Program.cpp.
  void send_interpreter_(std::int64_t t, double v, const std::string& port_id);

  // Vector-input interpreter fallback for send_vector(). Packs the lane values
  // into a Message<VectorNumberData> and dispatches to the entry operator.
  // Out-of-line for the same TU-isolation reasons as send_interpreter_.
  void send_vector_interpreter_(std::int64_t t, const double* v, std::size_t n,
                                const std::string& port_id);

  // Quick check: every message in the batch matches the JIT's input shape
  // (scalar NumberData when input_is_vector_==false, otherwise a
  // VectorNumberData with at least cached_jit_input_lane_width_ lanes).
  bool batch_is_jit_compatible_(
      const std::map<std::string, std::vector<std::unique_ptr<BaseMessage>>>& port_messages) const {
    const bool input_vec = jit_program_->input_is_vector();
    for (const auto& [port_id, messages] : port_messages) {
      for (const auto& msg : messages) {
        if (input_vec) {
          const auto* vm = dynamic_cast<const Message<VectorNumberData>*>(msg.get());
          if (!vm) return false;
          if (vm->data.values->size() < cached_jit_input_lane_width_) return false;
        } else {
          if (!dynamic_cast<const Message<NumberData>*>(msg.get())) return false;
        }
      }
    }
    return true;
  }

  void send_to_entry(std::unique_ptr<BaseMessage> msg, const std::string& port_id, bool debug = false) {
    auto port_info = OperatorJson::parse_port_name(port_id);
    operators_[entry_operator_id_]->receive_data(std::move(msg), port_info.index);
    operators_[entry_operator_id_]->execute(debug);
  }

  void send_batch_to_entry(
      const std::map<std::string, std::vector<std::unique_ptr<BaseMessage>>>& port_messages, bool debug) {
    auto& entry = operators_[entry_operator_id_];
    for (const auto& [port_id, messages] : port_messages) {
      if (messages.empty()) continue;
      auto port_info = OperatorJson::parse_port_name(port_id);
      // Clone the caller's batch into a local vector so receive_data_batch can
      // take ownership. Operators that override the batch path (e.g.
      // BurstAggregate) then skip the per-message queue hop entirely.
      std::vector<std::unique_ptr<BaseMessage>> cloned;
      cloned.reserve(messages.size());
      for (const auto& msg : messages) {
        cloned.push_back(msg->clone());
      }
      entry->receive_data_batch(cloned, port_info.index, debug);
    }
    entry->execute(debug);
  }

  ProgramMsgBatch collect_outputs(bool debug_mode = false) {
    ProgramMsgBatch batch;

    if (!debug_mode) {
      // Non-debug fast path: drain the single multi-port sink and read
      // provenance (upstream operator id + output port) from each port's
      // inbound connection ref.
      if (!sink_) return batch;

      const size_t n = sink_->num_data_ports();
      for (size_t p = 0; p < n; ++p) {
        auto& q = sink_->get_data_queue(p);
        if (q.empty()) continue;

        // Port p was wired by setup_output_sinks_() to exactly one upstream;
        // invariant enforced by the only writer of sink_.
        const auto& ref = sink_->inbound_data_refs(p).front();
        const auto& conn = ref.parent->get_connection(ref.conn_index);

        PortMsgBatch port_msgs;
        port_msgs.reserve(q.size());
        for (auto& msg : q) port_msgs.push_back(std::move(msg));
        q.clear();

        batch[ref.parent->id()]["o" + std::to_string(conn.output_port + 1)] =
            std::move(port_msgs);
      }
      return batch;
    }

    // Debug mode: walk every operator (including composite children) and
    // collect from debug output queues.
    std::function<void(const std::string&, const std::shared_ptr<Operator>&, const std::string&)>
        collect_operator_outputs =
            [&](const std::string& op_id, const std::shared_ptr<Operator>& op, const std::string& parent_prefix) {
              std::string qualified_id = parent_prefix.empty() ? op_id : parent_prefix + "::" + op_id;

              OperatorMsgBatch op_batch;
              bool has_messages = false;

              for (size_t i = 0; i < op->num_output_ports(); i++) {
                const auto& queue = op->get_debug_output_queue(i);
                if (!queue.empty()) {
                  PortMsgBatch port_msgs;
                  for (const auto& msg : queue) {
                    port_msgs.push_back(msg->clone());
                  }
                  op_batch["o" + std::to_string(i + 1)] = std::move(port_msgs);
                  has_messages = true;
                }
              }

              if (has_messages) {
                batch[qualified_id] = std::move(op_batch);
              }

              // Recursively handle composite operators (Pipeline, TriggerSet).
              if (const auto* kids = op->children_ops()) {
                for (const auto& [internal_id, internal_op] : *kids) {
                  collect_operator_outputs(internal_id, internal_op, qualified_id);
                }
              }
            };

    for (const auto& [op_id, op] : operators_) {
      collect_operator_outputs(op_id, op, "");
    }

    return batch;
  }
};

// Program Manager class to handle multiple programs
class ProgramManager {
 public:
  static ProgramManager& instance() {
    static ProgramManager instance;
    return instance;
  }

  void clear_all_programs() {
    programs_.clear();
    message_buffer_.clear();
    vector_builders_.clear();
  }

  string create_program(const string& id, const string& json_program) {
    if (programs_.count(id) > 0) {
      std::cout << "Program " << id << " already exists, count = " << programs_.count(id) << std::endl;
      return "Program " + id + " already exists";
    }
    try {
      programs_.emplace(id, Program(json_program));
      return "";
    } catch (const exception& e) {
      return string("Failed to create program: ") + e.what();
    }
  }

  bool add_to_message_buffer(const string& program_id, const string& port_id, const Message<NumberData>& msg) {
    if (programs_.count(program_id) == 0) return false;
    message_buffer_[program_id][port_id].push_back(create_message<NumberData>(msg.time, msg.data));
    return true;
  }

  bool begin_vector_message(const string& program_id, const string& port_id, timestamp_t time) {
    if (programs_.count(program_id) == 0) return false;
    auto& builder = vector_builders_[program_id][port_id];
    if (builder.active) return false;
    builder.active = true;
    builder.time = time;
    builder.values.clear();
    return true;
  }

  bool push_vector_message_value(const string& program_id, const string& port_id, double value) {
    auto prog_it = vector_builders_.find(program_id);
    if (prog_it == vector_builders_.end()) return false;
    auto port_it = prog_it->second.find(port_id);
    if (port_it == prog_it->second.end() || !port_it->second.active) return false;
    port_it->second.values.push_back(value);
    return true;
  }

  bool end_vector_message(const string& program_id, const string& port_id) {
    if (programs_.count(program_id) == 0) return false;
    auto prog_it = vector_builders_.find(program_id);
    if (prog_it == vector_builders_.end()) return false;
    auto port_it = prog_it->second.find(port_id);
    if (port_it == prog_it->second.end() || !port_it->second.active) return false;

    auto& builder = port_it->second;
    message_buffer_[program_id][port_id].push_back(
        create_message<VectorNumberData>(builder.time, VectorNumberData{builder.values}));
    builder.active = false;
    builder.values.clear();
    return true;
  }

  bool abort_vector_message(const string& program_id, const string& port_id) {
    auto prog_it = vector_builders_.find(program_id);
    if (prog_it == vector_builders_.end()) return false;
    auto port_it = prog_it->second.find(port_id);
    if (port_it == prog_it->second.end() || !port_it->second.active) return false;
    port_it->second.active = false;
    port_it->second.values.clear();
    return true;
  }

  ProgramMsgBatch process_message_buffer(const string& program_id) {
    if (programs_.count(program_id) == 0) {
      throw runtime_error("Program " + program_id + " not found");
    }

    auto& prog = programs_.at(program_id);
    auto buffer_it = message_buffer_.find(program_id);
    if (buffer_it == message_buffer_.end() || buffer_it->second.empty()) {
      return {};
    }

    ProgramMsgBatch result = prog.receive_batch(buffer_it->second);
    message_buffer_.erase(buffer_it);
    return result;
  }

  ProgramMsgBatch process_message_buffer_debug(const string& program_id) {
    if (programs_.count(program_id) == 0) {
      throw runtime_error("Program " + program_id + " not found");
    }

    auto& prog = programs_.at(program_id);
    auto buffer_it = message_buffer_.find(program_id);
    if (buffer_it == message_buffer_.end() || buffer_it->second.empty()) {
      return {};
    }

    ProgramMsgBatch result = prog.receive_batch_debug(buffer_it->second);
    message_buffer_.erase(buffer_it);
    return result;
  }

  string get_program_entry_operator_id(const string& program_id) {
    try {
      return get_program(program_id).get_entry_operator();
    } catch (const exception&) {
      return "";
    }
  }

  string serialize_program_data(const string& program_id) { return get_program(program_id).serialize_data(); }

  void restore_program_data_from_json(const string& program_id, const string& json_state) {
    get_program(program_id).restore_data_from_json(json_state);
  }

  bool delete_program(const string& program_id) {
    vector_builders_.erase(program_id);
    return programs_.erase(program_id) > 0;
  }

 private:
  Program& get_program(const string& program_id) {
    auto it = programs_.find(program_id);
    if (it == programs_.end()) {
      throw runtime_error("Program " + program_id + " not found");
    }
    return it->second;
  }

  void merge_batches(ProgramMsgBatch& target, const ProgramMsgBatch& source) {
    for (const auto& [op_id, op_batch] : source) {
      for (const auto& [port_name, port_msgs] : op_batch) {
        auto& target_port = target[op_id][port_name];
        target_port.reserve(target_port.size() + port_msgs.size());
        for (const auto& msg : port_msgs) {
          target_port.push_back(std::move(msg->clone()));
        }
      }
    }
  }

  struct VectorBuilderState {
    bool active = false;
    timestamp_t time = 0;
    std::vector<double> values;
  };

  map<string, Program> programs_;
  map<string, map<string, vector<std::unique_ptr<BaseMessage>>>> message_buffer_;
  map<string, map<string, VectorBuilderState>> vector_builders_;
};

}  // namespace rtbot

#endif  // PROGRAM_H
