#ifndef RTBOT_FUSE_BURST_AGGREGATE_H
#define RTBOT_FUSE_BURST_AGGREGATE_H

#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedScalarEval.h"

namespace rtbot {

// Burst-oriented aggregator.
//
// Runs an aggregate fused-bytecode program (CUMSUM / COUNT / MAX_AGG / etc.)
// over a stream of VectorNumberData rows and emits one aggregated output per
// segment-predicate transition. Designed to collapse the
// `KeyedPipeline → Pipeline → FusedExpressionVector` chain that today handles
// single-stream segment-grouped aggregations into a single operator: a burst
// of N incoming rows is processed in one tight loop, with no per-row Message
// allocation inside the operator and no dispatch between aggregate updates.
//
// Output shape: `[key_column_0, ..., key_column_{K-1},
//                 agg_0, ..., agg_{M-1}]` emitted at segment boundaries.
// Key columns come from the input row (pass-through, most recent value);
// aggregate columns come from the aggregate bytecode's END markers.
//
// Segment detection is driven by an optional boolean predicate bytecode. A
// transition fires when the predicate value for the current row differs from
// the previous row's; on transition the operator emits the aggregate as
// accumulated over the previous segment, then resets aggregate state and
// continues. When the predicate bytecode is empty, no segment transitions
// fire and the operator accumulates indefinitely (callers emit externally).
class BurstAggregate : public Operator {
 public:
  BurstAggregate(std::string id,
                 std::vector<double> agg_bytecode,
                 std::vector<double> agg_constants,
                 std::vector<double> seg_bytecode,
                 std::vector<double> seg_constants,
                 std::vector<std::size_t> key_columns,
                 std::size_t num_agg_outputs,
                 std::size_t num_input_cols)
      : Operator(std::move(id)),
        agg_constants_(std::move(agg_constants)),
        seg_constants_(std::move(seg_constants)),
        key_columns_(std::move(key_columns)),
        num_agg_outputs_(num_agg_outputs),
        num_input_cols_(num_input_cols) {
    auto agg_pack = rtbot::fuse::pack_bytecode(agg_bytecode);
    agg_packed_ = std::move(agg_pack.packed);
    agg_aux_ = std::move(agg_pack.aux_args);
    agg_state_init_ = std::move(agg_pack.state_init);
    agg_state_ = agg_state_init_;
    agg_out_.resize(num_agg_outputs_);

    if (!seg_bytecode.empty()) {
      auto seg_pack = rtbot::fuse::pack_bytecode(seg_bytecode);
      seg_packed_ = std::move(seg_pack.packed);
      seg_aux_ = std::move(seg_pack.aux_args);
      // Segment predicate is stateless in V1 — ignore seg_pack.state_init.
      seg_scratch_.resize(1);
    }

    last_key_values_.assign(key_columns_.size(), 0.0);
    last_valid_output_.assign(num_agg_outputs_, 0.0);

    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();
  }

  std::string type_name() const override { return "BurstAggregate"; }
  std::size_t get_num_agg_outputs() const { return num_agg_outputs_; }
  std::size_t get_num_input_cols() const { return num_input_cols_; }
  const std::vector<std::size_t>& get_key_columns() const { return key_columns_; }

  void reset() override {
    Operator::reset();
    agg_state_ = agg_state_init_;
    has_seg_value_ = false;
    last_seg_value_ = 0.0;
    has_valid_output_ = false;
  }

  // Batch entry point — the whole point of this operator. Unpack each row's
  // amplitude fields directly, run segment + aggregate bytecodes in-place,
  // emit on segment transitions. Bypasses the per-message queue hop that the
  // default Operator::receive_data_batch would otherwise pay.
  void receive_data_batch(std::vector<std::unique_ptr<BaseMessage>>& messages,
                          size_t port_index, bool debug) override {
    (void)port_index;
    for (auto& msg : messages) {
      auto* vm = static_cast<Message<VectorNumberData>*>(msg.get());
      if (!vm) continue;
      const auto& row = *vm->data.values;
      process_row_(vm->time, row.data(), row.size(), debug);
    }
    messages.clear();
  }

  // Raw-buffer entry point — zero Message allocations. Producers that can
  // ship row-major double buffers (sensor drivers, file replay, WASM
  // ArrayBuffer) hit this path and let BurstAggregate stream directly over
  // the input memory.
  void receive_data_buffer(const double* data, size_t num_rows,
                            size_t num_cols, const timestamp_t* times,
                            size_t port_index, bool debug) override {
    (void)port_index;
    if (num_cols < num_input_cols_) {
      throw std::runtime_error(
          "BurstAggregate buffer has " + std::to_string(num_cols) +
          " cols but expected at least " + std::to_string(num_input_cols_));
    }
    for (size_t r = 0; r < num_rows; ++r) {
      process_row_(times[r], data + r * num_cols, num_cols, debug);
    }
  }

 protected:
  // Fallback path — when the caller uses the per-message receive_data API,
  // process_data drains the queued messages through the same row loop.
  void process_data(bool debug = false) override {
    auto& queue = get_data_queue(0);
    while (!queue.empty()) {
      auto* vm = static_cast<const Message<VectorNumberData>*>(queue.front().get());
      const auto& row = *vm->data.values;
      process_row_(vm->time, row.data(), row.size(), debug);
      queue.pop_front();
    }
  }

 private:
  void process_row_(timestamp_t time, const double* row, std::size_t cols,
                    bool debug) {
    if (cols < num_input_cols_) {
      throw std::runtime_error(
          "BurstAggregate row has " + std::to_string(cols) +
          " columns but expected at least " + std::to_string(num_input_cols_));
    }

    // Capture key columns before any potential emission so the output carries
    // the most recent key values (constant within a segment).
    for (std::size_t k = 0; k < key_columns_.size(); ++k) {
      last_key_values_[k] = row[key_columns_[k]];
    }

    // Segment detection. A transition in the predicate value fires emission
    // of the aggregate accumulated over the previous segment, stamped with
    // the transitioning row's timestamp.
    if (!seg_packed_.empty()) {
      double pred = 0.0;
      rtbot::fuse::evaluate_one(
          seg_packed_.data(), seg_packed_.size(),
          seg_constants_.empty() ? nullptr : seg_constants_.data(),
          seg_aux_.empty() ? nullptr : seg_aux_.data(),
          nullptr, row, nullptr, &pred, 1);
      if (has_seg_value_ && pred != last_seg_value_) {
        if (has_valid_output_) emit_current_(time, debug);
        agg_state_ = agg_state_init_;
      }
      last_seg_value_ = pred;
      has_seg_value_ = true;
    }

    // Aggregate update. FE's aggregate bytecode emits the running aggregate
    // value(s) per invocation; we retain the most recent output so the next
    // segment-boundary emission uses the final value of the closed segment.
    rtbot::fuse::evaluate_one(
        agg_packed_.data(), agg_packed_.size(),
        agg_constants_.empty() ? nullptr : agg_constants_.data(),
        agg_aux_.empty() ? nullptr : agg_aux_.data(),
        nullptr, row, agg_state_.data(), agg_out_.data(),
        num_agg_outputs_);
    std::copy(agg_out_.begin(), agg_out_.end(), last_valid_output_.begin());
    has_valid_output_ = true;
  }

  void emit_current_(timestamp_t time, bool debug) {
    auto out_vec = make_pooled_vector_double(
        key_columns_.size() + num_agg_outputs_);
    std::size_t idx = 0;
    for (std::size_t k = 0; k < key_columns_.size(); ++k) {
      (*out_vec)[idx++] = last_key_values_[k];
    }
    for (std::size_t j = 0; j < num_agg_outputs_; ++j) {
      (*out_vec)[idx++] = last_valid_output_[j];
    }
    emit_output(0,
                create_message<VectorNumberData>(
                    time, VectorNumberData(std::move(out_vec))),
                debug);
    has_valid_output_ = false;
  }

  std::vector<rtbot::fuse::Instruction> agg_packed_;
  std::vector<rtbot::fuse::AuxArgs> agg_aux_;
  std::vector<double> agg_constants_;
  std::vector<double> agg_state_init_;
  std::vector<double> agg_state_;
  std::vector<double> agg_out_;
  std::size_t num_agg_outputs_;

  std::vector<rtbot::fuse::Instruction> seg_packed_;
  std::vector<rtbot::fuse::AuxArgs> seg_aux_;
  std::vector<double> seg_constants_;
  std::vector<double> seg_scratch_;
  bool has_seg_value_{false};
  double last_seg_value_{0.0};

  std::vector<std::size_t> key_columns_;
  std::vector<double> last_key_values_;
  std::vector<double> last_valid_output_;
  bool has_valid_output_{false};
  std::size_t num_input_cols_;
};

inline std::shared_ptr<BurstAggregate> make_burst_aggregate(
    std::string id,
    std::vector<double> agg_bytecode,
    std::vector<double> agg_constants,
    std::vector<double> seg_bytecode,
    std::vector<double> seg_constants,
    std::vector<std::size_t> key_columns,
    std::size_t num_agg_outputs,
    std::size_t num_input_cols) {
  return std::make_shared<BurstAggregate>(
      std::move(id), std::move(agg_bytecode), std::move(agg_constants),
      std::move(seg_bytecode), std::move(seg_constants),
      std::move(key_columns), num_agg_outputs, num_input_cols);
}

}  // namespace rtbot

#endif  // RTBOT_FUSE_BURST_AGGREGATE_H
