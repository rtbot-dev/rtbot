#ifndef FUSED_EXPRESSION_H
#define FUSED_EXPRESSION_H

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <array>

#include "rtbot/fuse/FusedBatchEval.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"
#include "rtbot/fuse/FusedScalarEval.h"
#include "rtbot/std/VectorCompose.h"

namespace rtbot {

// Opcode constants are defined in rtbot/fuse/FusedOps.h so that lower-level
// headers (FusedBytecode.h, FusedScalarEval.h) can reference them without a
// circular include on this header.

class FusedExpression : public VectorCompose {
 public:
  // num_ports: number of scalar NUMBER input ports (same as VectorCompose)
  // num_outputs: number of output columns in the emitted VectorNumberData
  // bytecode: flat double array encoding M expression trees in postfix notation
  // constants: flat double array of compile-time constants referenced by bytecode
  // state_init: initial values for persistent state slots (empty = pure expressions)
  FusedExpression(std::string id, size_t num_ports, size_t num_outputs,
                  std::vector<double> bytecode, std::vector<double> constants,
                  std::vector<double> state_init = {},
                  size_t max_size_per_port = MAX_SIZE_PER_PORT)
      : VectorCompose(std::move(id), num_ports, max_size_per_port),
        num_outputs_(num_outputs),
        bytecode_(std::move(bytecode)),
        constants_(std::move(constants)),
        state_init_(std::move(state_init)),
        state_(state_init_),
        packed_(rtbot::fuse::encode_legacy(bytecode_)),
        can_batch_(true) {
    // Programs with STATE_LOAD or Tier-1 windowed opcodes cannot yet run
    // through evaluate_batched (STATE_LOAD reads contaminated shared state
    // across lanes; windowed opcodes are not implemented in the batched
    // evaluator). Force scalar fallback for any of those — correctness first.
    for (const auto& i : packed_) {
      const auto op = i.op;
      if (op == static_cast<std::uint8_t>(fused_op::STATE_LOAD) ||
          (op >= static_cast<std::uint8_t>(fused_op::MA_UPDATE) &&
           op <= static_cast<std::uint8_t>(fused_op::IIR_UPDATE))) {
        can_batch_ = false;
        break;
      }
    }
    if (num_outputs_ < 1) {
      throw std::runtime_error(
          "FusedExpression requires at least 1 output expression");
    }
    // Validate bytecode has exactly num_outputs END markers.
    // Must walk opcodes respecting argument structure — INPUT, CONST, and
    // stateful opcodes consume the next double as an argument, so those
    // positions must not be counted as opcodes.
    size_t end_count = 0;
    size_t pc = 0;
    while (pc < bytecode_.size()) {
      int opcode = static_cast<int>(bytecode_[pc++]);
      if (opcode == static_cast<int>(fused_op::INPUT) ||
          opcode == static_cast<int>(fused_op::CONST) ||
          opcode == static_cast<int>(fused_op::CUMSUM) ||
          opcode == static_cast<int>(fused_op::COUNT) ||
          opcode == static_cast<int>(fused_op::MAX_AGG) ||
          opcode == static_cast<int>(fused_op::MIN_AGG) ||
          opcode == static_cast<int>(fused_op::STATE_LOAD)) {
        ++pc;  // skip argument
      } else if (opcode == static_cast<int>(fused_op::END)) {
        ++end_count;
      }
    }
    if (end_count != num_outputs_) {
      throw std::runtime_error(
          "FusedExpression bytecode has " + std::to_string(end_count) +
          " END markers but num_outputs is " + std::to_string(num_outputs_));
    }
  }

  std::string type_name() const override { return "FusedExpression"; }

  size_t get_num_outputs() const { return num_outputs_; }
  const std::vector<double>& get_bytecode() const { return bytecode_; }
  const std::vector<double>& get_constants() const { return constants_; }
  const std::vector<double>& get_state_init() const { return state_init_; }

  void reset() override {
    VectorCompose::reset();
    state_ = state_init_;
  }

  Bytes collect_bytes() override {
    Bytes bytes = VectorCompose::collect_bytes();
    size_t n = state_.size();
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&n),
                 reinterpret_cast<const uint8_t*>(&n) + sizeof(n));
    for (double v : state_) {
      bytes.insert(bytes.end(), reinterpret_cast<const uint8_t*>(&v),
                   reinterpret_cast<const uint8_t*>(&v) + sizeof(v));
    }
    return bytes;
  }

  void restore(Bytes::const_iterator& it) override {
    VectorCompose::restore(it);
    size_t n;
    std::memcpy(&n, &(*it), sizeof(n));
    it += sizeof(n);
    state_.resize(n);
    for (size_t i = 0; i < n; i++) {
      std::memcpy(&state_[i], &(*it), sizeof(double));
      it += sizeof(double);
    }
  }

 protected:
  void process_data(bool debug = false) override {
    const size_t np = get_num_ports();
    const rtbot::fuse::Instruction* ins = packed_.data();
    const size_t ins_size = packed_.size();
    const double* consts = constants_.data();

    if (!can_batch_) {
      // Scalar fast path — used for programs that read shared state via
      // STATE_LOAD (which has no correct lane-parallel semantics).
      double inputs[64];
      while (true) {
        const bool is_sync = sync_data_inputs();
        bool any_empty = false;
        for (size_t i = 0; i < np; i++) {
          if (get_data_queue(i).empty()) { any_empty = true; break; }
        }
        if (!is_sync && any_empty) return;
        if (!is_sync) continue;

        timestamp_t time = 0;
        for (size_t i = 0; i < np; i++) {
          const auto* msg = static_cast<const Message<NumberData>*>(
              get_data_queue(i).front().get());
          time = msg->time;
          inputs[i] = msg->data.value;
        }
        for (size_t i = 0; i < np; i++) get_data_queue(i).pop_front();

        auto out_vec = std::make_shared<std::vector<double>>(num_outputs_);
        const bool emit = rtbot::fuse::evaluate_one(
            ins, ins_size, consts, /*aux_args=*/nullptr,
            /*coefficients=*/nullptr, inputs, state_.data(),
            out_vec->data(), num_outputs_);
        if (emit) {
          get_output_queue(0).push_back(create_message<VectorNumberData>(
              time, VectorNumberData(std::move(out_vec))));
        }
      }
    }

    // Batched fast path.
    constexpr size_t B = rtbot::fuse::kBatch;
    std::array<std::array<double, B>, 64> batched_inputs{};
    std::array<timestamp_t, B> times{};
    std::vector<double> out_batch(B * num_outputs_, 0.0);

    while (true) {
      size_t lane = 0;
      while (lane < B) {
        const bool is_sync = sync_data_inputs();
        bool any_empty = false;
        for (size_t i = 0; i < np; i++) {
          if (get_data_queue(i).empty()) { any_empty = true; break; }
        }
        if (!is_sync && any_empty) break;
        if (!is_sync) continue;

        timestamp_t t = 0;
        for (size_t i = 0; i < np; i++) {
          const auto* msg = static_cast<const Message<NumberData>*>(
              get_data_queue(i).front().get());
          t = msg->time;
          batched_inputs[i][lane] = msg->data.value;
        }
        times[lane] = t;
        for (size_t i = 0; i < np; i++) get_data_queue(i).pop_front();
        ++lane;
      }

      if (lane == 0) return;

      if (lane == 1) {
        // Short-circuit the single-tuple case. The batched evaluator's
        // lane-parallel inner loops add ~40-50% overhead when active_lanes==1
        // because stack[64][B] layout and indirect accesses defeat the
        // scalar hot path. This branch recovers Phase 1 throughput under the
        // realistic per-message execute() pattern.
        double inputs1[64];
        for (size_t i = 0; i < np; ++i) inputs1[i] = batched_inputs[i][0];
        auto out_vec = std::make_shared<std::vector<double>>(num_outputs_);
        const bool emit = rtbot::fuse::evaluate_one(
            ins, ins_size, consts, /*aux_args=*/nullptr,
            /*coefficients=*/nullptr, inputs1, state_.data(),
            out_vec->data(), num_outputs_);
        if (emit) {
          get_output_queue(0).push_back(create_message<VectorNumberData>(
              times[0], VectorNumberData(std::move(out_vec))));
        }
        continue;
      }

      rtbot::fuse::evaluate_batched<B>(ins, ins_size, consts,
                                        /*aux_args=*/nullptr,
                                        /*coefficients=*/nullptr,
                                        batched_inputs.data(), lane,
                                        state_.data(), out_batch.data(),
                                        num_outputs_);

      for (size_t l = 0; l < lane; ++l) {
        auto out_vec = std::make_shared<std::vector<double>>(num_outputs_);
        std::copy(out_batch.begin() + l * num_outputs_,
                  out_batch.begin() + (l + 1) * num_outputs_,
                  out_vec->begin());
        get_output_queue(0).push_back(create_message<VectorNumberData>(
            times[l], VectorNumberData(std::move(out_vec))));
      }
    }
  }

 private:
  size_t num_outputs_;
  std::vector<double> bytecode_;
  std::vector<double> constants_;
  std::vector<double> state_init_;
  std::vector<double> state_;
  std::vector<rtbot::fuse::Instruction> packed_;
  bool can_batch_;
};

inline std::shared_ptr<FusedExpression> make_fused_expression(
    std::string id, size_t num_ports, size_t num_outputs,
    std::vector<double> bytecode, std::vector<double> constants,
    std::vector<double> state_init = {}) {
  return std::make_shared<FusedExpression>(std::move(id), num_ports,
                                            num_outputs, std::move(bytecode),
                                            std::move(constants),
                                            std::move(state_init));
}

}  // namespace rtbot

#endif  // FUSED_EXPRESSION_H
