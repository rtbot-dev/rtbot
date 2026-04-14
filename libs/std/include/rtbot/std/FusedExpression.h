#ifndef FUSED_EXPRESSION_H
#define FUSED_EXPRESSION_H

#include <cmath>
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
}  // namespace fused_op

class FusedExpression : public VectorCompose {
 public:
  // num_ports: number of scalar NUMBER input ports (same as VectorCompose)
  // num_outputs: number of output columns in the emitted VectorNumberData
  // bytecode: flat double array encoding M expression trees in postfix notation
  // constants: flat double array of compile-time constants referenced by bytecode
  FusedExpression(std::string id, size_t num_ports, size_t num_outputs,
                  std::vector<double> bytecode, std::vector<double> constants,
                  size_t max_size_per_port = MAX_SIZE_PER_PORT)
      : VectorCompose(std::move(id), num_ports, max_size_per_port),
        num_outputs_(num_outputs),
        bytecode_(std::move(bytecode)),
        constants_(std::move(constants)) {
    if (num_outputs_ < 1) {
      throw std::runtime_error(
          "FusedExpression requires at least 1 output expression");
    }
    // Validate bytecode has exactly num_outputs END markers
    size_t end_count = 0;
    for (double op : bytecode_) {
      if (static_cast<int>(op) == static_cast<int>(fused_op::END)) {
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

 protected:
  void process_data(bool debug = false) override {
    const size_t np = get_num_ports();

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

      // Read synced scalar values from input ports
      timestamp_t time = 0;
      std::vector<double> inputs(np);
      for (size_t i = 0; i < np; i++) {
        const auto* msg = static_cast<const Message<NumberData>*>(
            get_data_queue(i).front().get());
        if (!msg) {
          throw std::runtime_error(
              "Invalid message type in FusedExpression");
        }
        time = msg->time;
        inputs[i] = msg->data.value;
      }

      // Pop all input port fronts
      for (size_t i = 0; i < np; i++) {
        get_data_queue(i).pop_front();
      }

      // Evaluate bytecode to produce output values
      std::vector<double> outputs;
      outputs.reserve(num_outputs_);

      // RPN evaluation stack
      double stack[64];
      size_t sp = 0;

      size_t pc = 0;
      while (pc < bytecode_.size()) {
        int opcode = static_cast<int>(bytecode_[pc++]);

        switch (opcode) {
          case static_cast<int>(fused_op::INPUT): {
            int idx = static_cast<int>(bytecode_[pc++]);
            stack[sp++] = inputs[idx];
            break;
          }
          case static_cast<int>(fused_op::CONST): {
            int idx = static_cast<int>(bytecode_[pc++]);
            stack[sp++] = constants_[idx];
            break;
          }
          case static_cast<int>(fused_op::ADD): {
            double b = stack[--sp];
            double a = stack[--sp];
            stack[sp++] = a + b;
            break;
          }
          case static_cast<int>(fused_op::SUB): {
            double b = stack[--sp];
            double a = stack[--sp];
            stack[sp++] = a - b;
            break;
          }
          case static_cast<int>(fused_op::MUL): {
            double b = stack[--sp];
            double a = stack[--sp];
            stack[sp++] = a * b;
            break;
          }
          case static_cast<int>(fused_op::DIV): {
            double b = stack[--sp];
            double a = stack[--sp];
            stack[sp++] = a / b;
            break;
          }
          case static_cast<int>(fused_op::POW): {
            double b = stack[--sp];
            double a = stack[--sp];
            stack[sp++] = std::pow(a, b);
            break;
          }
          case static_cast<int>(fused_op::ABS): {
            stack[sp - 1] = std::abs(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::SQRT): {
            stack[sp - 1] = std::sqrt(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::LOG): {
            stack[sp - 1] = std::log(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::LOG10): {
            stack[sp - 1] = std::log10(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::EXP): {
            stack[sp - 1] = std::exp(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::SIN): {
            stack[sp - 1] = std::sin(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::COS): {
            stack[sp - 1] = std::cos(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::TAN): {
            stack[sp - 1] = std::tan(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::SIGN): {
            double v = stack[sp - 1];
            stack[sp - 1] = (v > 0.0) ? 1.0 : (v < 0.0) ? -1.0 : 0.0;
            break;
          }
          case static_cast<int>(fused_op::FLOOR): {
            stack[sp - 1] = std::floor(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::CEIL): {
            stack[sp - 1] = std::ceil(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::ROUND): {
            stack[sp - 1] = std::round(stack[sp - 1]);
            break;
          }
          case static_cast<int>(fused_op::NEG): {
            stack[sp - 1] = -stack[sp - 1];
            break;
          }
          case static_cast<int>(fused_op::END): {
            // Top of stack is the expression result
            outputs.push_back(stack[--sp]);
            sp = 0;  // reset stack for next expression
            break;
          }
          default:
            throw std::runtime_error(
                "FusedExpression: unknown opcode " + std::to_string(opcode));
        }
      }

      // Assemble output vector
      VectorNumberData result(std::move(outputs));
      get_output_queue(0).push_back(
          create_message<VectorNumberData>(time, std::move(result)));
    }
  }

 private:
  size_t num_outputs_;
  std::vector<double> bytecode_;
  std::vector<double> constants_;
};

inline std::shared_ptr<FusedExpression> make_fused_expression(
    std::string id, size_t num_ports, size_t num_outputs,
    std::vector<double> bytecode, std::vector<double> constants) {
  return std::make_shared<FusedExpression>(std::move(id), num_ports,
                                           num_outputs, std::move(bytecode),
                                           std::move(constants));
}

}  // namespace rtbot

#endif  // FUSED_EXPRESSION_H
