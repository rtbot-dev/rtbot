#ifndef FUSED_EXPRESSION_VECTOR_H
#define FUSED_EXPRESSION_VECTOR_H

#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Operator.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/fuse/FusedScalarEval.h"

namespace rtbot {

class FusedExpressionVector : public Operator {
 public:
  FusedExpressionVector(std::string id, size_t num_outputs,
                        std::vector<double> bytecode,
                        std::vector<double> constants,
                        std::vector<double> state_init = {},
                        size_t max_size_per_port = MAX_SIZE_PER_PORT)
      : Operator(std::move(id), max_size_per_port),
        num_outputs_(num_outputs),
        bytecode_(std::move(bytecode)),
        constants_(std::move(constants)),
        state_init_(std::move(state_init)),
        state_(state_init_),
        packed_(rtbot::fuse::encode_legacy(bytecode_)),
        min_required_input_size_(0) {
    // Compute once: the minimum inputs vector size this program needs.
    // Bytecode is immutable after construction so this can be cached.
    for (const auto& i : packed_) {
      if (i.op == static_cast<std::uint8_t>(fused_op::INPUT)) {
        std::size_t needed = static_cast<std::size_t>(i.arg) + 1;
        if (needed > min_required_input_size_) min_required_input_size_ = needed;
      }
    }
    add_data_port<VectorNumberData>();
    add_output_port<VectorNumberData>();

    if (num_outputs_ < 1) {
      throw std::runtime_error(
          "FusedExpressionVector requires at least 1 output expression");
    }

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
        ++pc;
      } else if (opcode == static_cast<int>(fused_op::END)) {
        ++end_count;
      }
    }

    if (end_count != num_outputs_) {
      throw std::runtime_error(
          "FusedExpressionVector bytecode has " + std::to_string(end_count) +
          " END markers but num_outputs is " + std::to_string(num_outputs_));
    }
  }

  std::string type_name() const override { return "FusedExpressionVector"; }

  size_t get_num_outputs() const { return num_outputs_; }
  const std::vector<double>& get_bytecode() const { return bytecode_; }
  const std::vector<double>& get_constants() const { return constants_; }
  const std::vector<double>& get_state_init() const { return state_init_; }

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
    return (num_outputs_ == other.num_outputs_ && bytecode_ == other.bytecode_ &&
            constants_ == other.constants_ && state_init_ == other.state_init_ &&
            state_ == other.state_ && Operator::equals(other));
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

    while (!input.empty()) {
      const auto* msg =
          static_cast<const Message<VectorNumberData>*>(input.front().get());
      timestamp_t time = msg->time;
      const std::vector<double>& inputs = *msg->data.values;

      // Single-word bounds check using the cached maximum INPUT index.
      // evaluate_one() does no bounds checking internally (the scalar
      // FusedExpression passes a fixed-size `inputs[num_ports]` array), so
      // the guard lives here — but it only needs to look at the incoming
      // vector's size, not walk the bytecode every message.
      if (inputs.size() < min_required_input_size_) {
        throw std::runtime_error(
            "FusedExpressionVector INPUT index out of bounds");
      }

      auto out_vec = std::make_shared<std::vector<double>>(num_outputs_);
      rtbot::fuse::evaluate_one(ins, ins_size, consts, inputs.data(),
                                 state_.data(), out_vec->data(), num_outputs_);

      get_output_queue(0).push_back(create_message<VectorNumberData>(
          time, VectorNumberData(std::move(out_vec))));
      input.pop_front();
    }
  }

 private:
  size_t num_outputs_;
  std::vector<double> bytecode_;
  std::vector<double> constants_;
  std::vector<double> state_init_;
  std::vector<double> state_;
  std::vector<rtbot::fuse::Instruction> packed_;
  std::size_t min_required_input_size_;
};

inline std::shared_ptr<FusedExpressionVector> make_fused_expression_vector(
    std::string id, size_t num_outputs, std::vector<double> bytecode,
    std::vector<double> constants, std::vector<double> state_init = {}) {
  return std::make_shared<FusedExpressionVector>(
      std::move(id), num_outputs, std::move(bytecode), std::move(constants),
      std::move(state_init));
}

}  // namespace rtbot

#endif  // FUSED_EXPRESSION_VECTOR_H
