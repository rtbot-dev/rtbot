#ifndef FUSED_EXPRESSION_H
#define FUSED_EXPRESSION_H

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/std/VectorCompose.h"

namespace rtbot {

// Bytecode opcodes for FusedExpression RPN evaluation.
// Each opcode is encoded as a double in the bytecode array.
// Opcodes with arguments consume the next double as the argument.
namespace fused_op {
constexpr double INPUT = 0;   // push v[next_double]
constexpr double CONST = 1;   // push constants[next_double]
constexpr double ADD = 2;     // pop b, a; push a+b
constexpr double SUB = 3;     // pop b, a; push a-b
constexpr double MUL = 4;     // pop b, a; push a*b
constexpr double DIV = 5;     // pop b, a; push a/b
constexpr double POW = 6;     // pop b, a; push pow(a,b)
constexpr double ABS = 7;     // pop a; push abs(a)
constexpr double SQRT = 8;    // pop a; push sqrt(a)
constexpr double LOG = 9;     // pop a; push log(a)
constexpr double LOG10 = 10;  // pop a; push log10(a)
constexpr double EXP = 11;    // pop a; push exp(a)
constexpr double SIN = 12;    // pop a; push sin(a)
constexpr double COS = 13;    // pop a; push cos(a)
constexpr double TAN = 14;    // pop a; push tan(a)
constexpr double SIGN = 15;   // pop a; push sign(a)
constexpr double FLOOR = 16;  // pop a; push floor(a)
constexpr double CEIL = 17;   // pop a; push ceil(a)
constexpr double ROUND = 18;  // pop a; push round(a)
constexpr double NEG = 19;    // pop a; push -a
constexpr double END = 20;    // end of expression, top of stack is result
constexpr double CUMSUM = 21;     // pop a; state[arg] += a (Kahan); push state[arg]. Uses 2 state slots: [sum, kahan_comp]
constexpr double COUNT = 22;      // state[arg] += 1; push state[arg]. Uses 1 state slot. Does NOT pop.
constexpr double MAX_AGG = 23;    // pop a; state[arg] = max(state[arg], a); push state[arg]. Uses 1 slot (init -inf)
constexpr double MIN_AGG = 24;    // pop a; state[arg] = min(state[arg], a); push state[arg]. Uses 1 slot (init +inf)
constexpr double STATE_LOAD = 25; // push state[arg] (read-only, no modification). For shared COUNT references.
constexpr double GT = 26;        // pop b, a; push (a > b) ? 1.0 : 0.0
constexpr double GTE = 27;       // pop b, a; push (a >= b) ? 1.0 : 0.0
constexpr double LT = 28;        // pop b, a; push (a < b) ? 1.0 : 0.0
constexpr double LTE = 29;       // pop b, a; push (a <= b) ? 1.0 : 0.0
constexpr double EQ = 30;        // pop b, a; push (a == b) ? 1.0 : 0.0
constexpr double NEQ = 31;       // pop b, a; push (a != b) ? 1.0 : 0.0
constexpr double AND = 32;       // pop b, a; push (a != 0.0 && b != 0.0) ? 1.0 : 0.0
constexpr double OR = 33;        // pop b, a; push (a != 0.0 || b != 0.0) ? 1.0 : 0.0
constexpr double NOT = 34;       // pop a; push (a == 0.0) ? 1.0 : 0.0
}  // namespace fused_op

class FusedExpression : public VectorCompose {
 public:
  // num_ports: number of scalar NUMBER input ports (same as VectorCompose)
  // num_outputs: number of output columns in the emitted VectorNumberData
  // bytecode: flat double array encoding M expression trees in postfix notation
  // constants: flat double array of compile-time constants referenced by bytecode
  // state_init: initial values for persistent state slots (empty = pure expressions)
  FusedExpression(std::string id, size_t num_ports, size_t num_outputs,
                  std::vector<double> bytecode, std::vector<double> constants,
                  std::vector<double> state_init = {})
      : VectorCompose(std::move(id), num_ports),
        num_outputs_(num_outputs),
        bytecode_(std::move(bytecode)),
        constants_(std::move(constants)),
        state_init_(std::move(state_init)),
        state_(state_init_) {
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
    const double* bc = bytecode_.data();
    const size_t bc_size = bytecode_.size();
    const double* consts = constants_.data();

    while (true) {
      // Use the built-in sync mechanism (inherited from Join via VectorCompose)
      bool is_any_empty = false;
      bool is_sync = sync_data_inputs();
      for (size_t i = 0; i < np; i++) {
        if (get_data_queue(i).empty()) {
          is_any_empty = true;
          break;
        }
      }
      if (!is_sync && is_any_empty) return;
      if (!is_sync) continue;

      // Read synced scalar values from input ports — stack-allocated
      timestamp_t time = 0;
      double inputs[64];
      for (size_t i = 0; i < np; i++) {
        const auto* msg = static_cast<const Message<NumberData>*>(
            get_data_queue(i).front().get());
        time = msg->time;
        inputs[i] = msg->data.value;
      }

      // Pop all input port fronts
      for (size_t i = 0; i < np; i++) {
        get_data_queue(i).pop_front();
      }

      // Allocate output vector once — write results directly into it
      auto out_vec = std::make_shared<std::vector<double>>(num_outputs_);
      double* out_ptr = out_vec->data();
      size_t out_idx = 0;

      // RPN evaluation stack (stack-allocated)
      double stack[64];
      size_t sp = 0;

      size_t pc = 0;
      while (pc < bc_size) {
        int opcode = static_cast<int>(bc[pc++]);

        switch (opcode) {
          case 0 /* INPUT */: {
            stack[sp++] = inputs[static_cast<int>(bc[pc++])];
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
                "FusedExpression: unknown opcode " + std::to_string(opcode));
        }
      }

      // Emit output — single allocation (the make_shared above)
      emit_output(0, create_message<VectorNumberData>(time, VectorNumberData(std::move(out_vec))), debug);
    }
  }

 private:
  size_t num_outputs_;
  std::vector<double> bytecode_;
  std::vector<double> constants_;
  std::vector<double> state_init_;
  std::vector<double> state_;
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
