#ifndef FUSED_EXPRESSION_VECTOR_H
#define FUSED_EXPRESSION_VECTOR_H

#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <array>

#include "rtbot/Operator.h"
#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedBatchEval.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/fuse/FusedScalarEval.h"
#include "rtbot/fuse/FusedStateLayout.h"

namespace rtbot {

class FusedExpressionVector : public Operator {
 public:
  FusedExpressionVector(std::string id, size_t num_outputs,
                        std::vector<double> bytecode,
                        std::vector<double> constants,
                        std::vector<double> coefficients = {},
                        std::vector<double> state_init = {})
      : Operator(std::move(id)),
        num_outputs_(num_outputs),
        constants_(std::move(constants)),
        coefficients_(std::move(coefficients)),
        min_required_input_size_(0),
        can_batch_(true) {
    auto pack = rtbot::fuse::pack_bytecode(bytecode);
    packed_ = std::move(pack.packed);
    aux_args_ = std::move(pack.aux_args);
    state_init_ = std::move(pack.state_init);
    if (!state_init.empty()) {
      if (state_init.size() > state_init_.size()) state_init_.resize(state_init.size(), 0.0);
      for (std::size_t k = 0; k < state_init.size(); ++k) state_init_[k] = state_init[k];
    }
    state_ = state_init_;
    // Compute once: the minimum inputs vector size this program needs,
    // and whether the program can be safely batched. Bytecode is immutable
    // after construction so this is a one-shot walk.
    for (const auto& i : packed_) {
      if (i.op == static_cast<std::uint8_t>(fused_op::INPUT)) {
        std::size_t needed = static_cast<std::size_t>(i.arg) + 1;
        if (needed > min_required_input_size_) min_required_input_size_ = needed;
      }
      // Scalar fallback for STATE_LOAD (shared-state contamination across
      // lanes) and Tier-1 windowed opcodes (serial-per-lane only — batching
      // adds overhead without vectorization wins). See the matching comment
      // in FusedExpression.h for rationale.
      const auto op = i.op;
      if (op == static_cast<std::uint8_t>(fused_op::STATE_LOAD) ||
          (op >= static_cast<std::uint8_t>(fused_op::MA_UPDATE) &&
           op <= static_cast<std::uint8_t>(fused_op::IIR_UPDATE))) {
        can_batch_ = false;
      }
    }
    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();

    if (num_outputs_ < 1) {
      throw std::runtime_error(
          "FusedExpressionVector requires at least 1 output expression");
    }

    size_t end_count = 0;
    for (const auto& i : packed_) {
      if (i.op == static_cast<std::uint8_t>(fused_op::END)) ++end_count;
    }

    if (end_count != num_outputs_) {
      throw std::runtime_error(
          "FusedExpressionVector bytecode has " + std::to_string(end_count) +
          " END markers but num_outputs is " + std::to_string(num_outputs_));
    }
  }

  std::string type_name() const override { return "FusedExpressionVector"; }

  size_t get_num_outputs() const { return num_outputs_; }
  std::vector<double> get_bytecode() const {
    return rtbot::fuse::unpack_bytecode(packed_, aux_args_);
  }
  const std::vector<rtbot::fuse::Instruction>& get_packed() const {
    return packed_;
  }
  const std::vector<double>& get_constants() const { return constants_; }
  const std::vector<double>& get_coefficients() const { return coefficients_; }

  void reset() override {
    Operator::reset();
    state_ = state_init_;
  }

  Bytes collect_bytes() override {
    Bytes bytes = Operator::collect_bytes();
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
    Operator::restore(it);
    size_t n;
    std::memcpy(&n, &(*it), sizeof(n));
    it += sizeof(n);
    state_.resize(n);
    for (size_t i = 0; i < n; i++) {
      std::memcpy(&state_[i], &(*it), sizeof(double));
      it += sizeof(double);
    }
  }

  bool equals(const FusedExpressionVector& other) const {
    if (packed_.size() != other.packed_.size()) return false;
    for (std::size_t k = 0; k < packed_.size(); ++k) {
      if (packed_[k].op != other.packed_[k].op ||
          packed_[k].flags != other.packed_[k].flags ||
          packed_[k].arg != other.packed_[k].arg) {
        return false;
      }
    }
    return (num_outputs_ == other.num_outputs_ &&
            constants_ == other.constants_ && state_ == other.state_ &&
            Operator::equals(other));
  }

  bool operator==(const FusedExpressionVector& other) const {
    return equals(other);
  }

  bool operator!=(const FusedExpressionVector& other) const {
    return !(*this == other);
  }

 protected:
  void process_data(bool debug = false) override {
    (void)debug;
    auto& input = get_data_queue(0);

    const rtbot::fuse::Instruction* ins = packed_.data();
    const size_t ins_size = packed_.size();
    const double* consts = constants_.data();

    if (!can_batch_) {
      while (!input.empty()) {
        const auto* msg = static_cast<const Message<VectorNumberData>*>(
            input.front().get());
        timestamp_t time = msg->time;
        const std::vector<double>& inputs = *msg->data.values;
        if (inputs.size() < min_required_input_size_) {
          throw std::runtime_error(
              "FusedExpressionVector INPUT index out of bounds");
        }
        auto out_vec = make_pooled_vector_double(num_outputs_);
        const bool emit = rtbot::fuse::evaluate_one(
            ins, ins_size, consts,
            aux_args_.empty() ? nullptr : aux_args_.data(),
            coefficients_.empty() ? nullptr : coefficients_.data(),
            inputs.data(), state_.data(),
            out_vec->data(), num_outputs_);
        if (emit) {
          emit_output(0,
                      create_message<VectorNumberData>(
                          time, VectorNumberData(std::move(out_vec))),
                      debug);
        }
        input.pop_front();
      }
      return;
    }

    constexpr size_t B = rtbot::fuse::kBatch;
    std::array<std::array<double, B>, 64> batched_inputs{};
    std::array<timestamp_t, B> times{};
    std::vector<double> out_batch(B * num_outputs_, 0.0);

    while (!input.empty()) {
      size_t lane = 0;
      while (lane < B && !input.empty()) {
        const auto* msg = static_cast<const Message<VectorNumberData>*>(
            input.front().get());
        const std::vector<double>& values = *msg->data.values;
        if (values.size() < min_required_input_size_) {
          throw std::runtime_error(
              "FusedExpressionVector INPUT index out of bounds");
        }
        for (size_t p = 0; p < min_required_input_size_; ++p)
          batched_inputs[p][lane] = values[p];
        times[lane] = msg->time;
        input.pop_front();
        ++lane;
      }

      if (lane == 0) return;

      if (lane == 1) {
        // Short-circuit single-tuple case — recovers scalar throughput when
        // no backlog is available (the realistic per-message execute pattern).
        double inputs1[64];
        for (size_t p = 0; p < min_required_input_size_; ++p)
          inputs1[p] = batched_inputs[p][0];
        auto out_vec = make_pooled_vector_double(num_outputs_);
        const bool emit = rtbot::fuse::evaluate_one(
            ins, ins_size, consts,
            aux_args_.empty() ? nullptr : aux_args_.data(),
            coefficients_.empty() ? nullptr : coefficients_.data(),
            inputs1, state_.data(),
            out_vec->data(), num_outputs_);
        if (emit) {
          emit_output(0,
                      create_message<VectorNumberData>(
                          times[0], VectorNumberData(std::move(out_vec))),
                      debug);
        }
        continue;
      }

      std::array<bool, B> lane_emit;
      lane_emit.fill(true);
      rtbot::fuse::evaluate_batched<B>(
          ins, ins_size, consts,
          aux_args_.empty() ? nullptr : aux_args_.data(),
          coefficients_.empty() ? nullptr : coefficients_.data(),
          batched_inputs.data(), lane, state_.data(), out_batch.data(),
          num_outputs_, lane_emit);

      for (size_t l = 0; l < lane; ++l) {
        if (!lane_emit[l]) continue;
        auto out_vec = make_pooled_vector_double(num_outputs_);
        std::copy(out_batch.begin() + l * num_outputs_,
                  out_batch.begin() + (l + 1) * num_outputs_,
                  out_vec->begin());
        emit_output(0,
                    create_message<VectorNumberData>(
                        times[l], VectorNumberData(std::move(out_vec))),
                    debug);
      }
    }
  }

 private:
  size_t num_outputs_;
  std::vector<double> constants_;
  std::vector<double> state_init_;
  std::vector<double> state_;
  std::vector<rtbot::fuse::Instruction> packed_;
  std::vector<rtbot::fuse::AuxArgs> aux_args_;
  std::vector<double> coefficients_;
  std::size_t min_required_input_size_;
  bool can_batch_;
};

inline std::shared_ptr<FusedExpressionVector> make_fused_expression_vector(
    std::string id, size_t num_outputs, std::vector<double> bytecode,
    std::vector<double> constants,
    std::vector<double> coefficients = {},
    std::vector<double> state_init = {}) {
  return std::make_shared<FusedExpressionVector>(
      std::move(id), num_outputs, std::move(bytecode), std::move(constants),
      std::move(coefficients), std::move(state_init));
}

}  // namespace rtbot

#endif  // FUSED_EXPRESSION_VECTOR_H
