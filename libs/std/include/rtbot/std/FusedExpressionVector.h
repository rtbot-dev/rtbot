#ifndef FUSED_EXPRESSION_VECTOR_H
#define FUSED_EXPRESSION_VECTOR_H

#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Operator.h"
#include "rtbot/std/FusedExpression.h"

namespace rtbot {

class FusedExpressionVector : public Operator {
 public:
  FusedExpressionVector(std::string id, size_t num_outputs,
                        std::vector<double> bytecode,
                        std::vector<double> constants,
                        std::vector<double> state_init = {})
      : Operator(std::move(id)),
        num_outputs_(num_outputs),
        bytecode_(std::move(bytecode)),
        constants_(std::move(constants)),
        state_init_(std::move(state_init)),
        state_(state_init_) {
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
    auto& input = get_data_queue(0);
    if (input.empty()) return;

    const double* bc = bytecode_.data();
    const size_t bc_size = bytecode_.size();
    const double* consts = constants_.data();

    const bool use_batch = input.size() >= kEmitBatchThreshold;
    std::vector<std::unique_ptr<BaseMessage>> batch;
    if (use_batch) batch.reserve(input.size());

    while (!input.empty()) {
      const auto* msg =
          static_cast<const Message<VectorNumberData>*>(input.front().get());
      timestamp_t time = msg->time;
      const std::vector<double>& inputs = *msg->data.values;

      auto out_vec = std::make_shared<std::vector<double>>(num_outputs_);
      double* out_ptr = out_vec->data();
      size_t out_idx = 0;

      double stack[64];
      size_t sp = 0;

      size_t pc = 0;
      while (pc < bc_size) {
        int opcode = static_cast<int>(bc[pc++]);

        switch (opcode) {
          case 0 /* INPUT */: {
            int input_index = static_cast<int>(bc[pc++]);
            if (input_index < 0 ||
                static_cast<size_t>(input_index) >= inputs.size()) {
              throw std::runtime_error(
                  "FusedExpressionVector INPUT index out of bounds");
            }
            stack[sp++] = inputs[static_cast<size_t>(input_index)];
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
            out_ptr[out_idx++] = stack[--sp];
            sp = 0;
            break;
          }
          case 21 /* CUMSUM */: {
            int si = static_cast<int>(bc[pc++]);
            double y = stack[--sp] - state_[si + 1];
            double t = state_[si] + y;
            state_[si + 1] = (t - state_[si]) - y;
            state_[si] = t;
            stack[sp++] = state_[si];
            break;
          }
          case 22 /* COUNT */: {
            int si = static_cast<int>(bc[pc++]);
            state_[si] += 1.0;
            stack[sp++] = state_[si];
            break;
          }
          case 23 /* MAX_AGG */: {
            int si = static_cast<int>(bc[pc++]);
            double v = stack[--sp];
            if (v > state_[si]) state_[si] = v;
            stack[sp++] = state_[si];
            break;
          }
          case 24 /* MIN_AGG */: {
            int si = static_cast<int>(bc[pc++]);
            double v = stack[--sp];
            if (v < state_[si]) state_[si] = v;
            stack[sp++] = state_[si];
            break;
          }
          case 25 /* STATE_LOAD */: {
            int si = static_cast<int>(bc[pc++]);
            stack[sp++] = state_[si];
            break;
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
                "FusedExpressionVector: unknown opcode " +
                std::to_string(opcode));
        }
      }

      if (use_batch) {
        batch.push_back(create_message<VectorNumberData>(
            time, VectorNumberData(std::move(out_vec))));
      } else {
        emit_output(0, create_message<VectorNumberData>(
            time, VectorNumberData(std::move(out_vec))), debug);
      }
      input.pop_front();
    }

    if (use_batch) {
      emit_output(0, std::move(batch), debug);
    }
  }

 private:
  size_t num_outputs_;
  std::vector<double> bytecode_;
  std::vector<double> constants_;
  std::vector<double> state_init_;
  std::vector<double> state_;
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
