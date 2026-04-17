#ifndef RTBOT_FUSE_SCALAR_EVAL_H
#define RTBOT_FUSE_SCALAR_EVAL_H

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

#include "rtbot/fuse/FusedBytecode.h"

// NOTE: opcode numeric literals below correspond to the `rtbot::fused_op`
// namespace constants declared in FusedExpression.h. FusedBytecode.h pulls in
// FusedExpression.h transitively.

namespace rtbot::fuse {

// Single-tuple scalar RPN evaluator.
//
// Evaluates the bytecode program against one synchronized input tuple, writing
// `num_outputs` results to `out_ptr` and mutating `state` for stateful opcodes.
//
// Used as the scalar hot path by both FusedExpression (which loops per
// synchronized scalar tuple) and FusedExpressionVector (which loops per
// incoming vector message). Also serves as the frozen oracle against which
// Phase 2+ batched / SIMD variants are parity-tested.
//
// Contract:
//   - `ins[ins_size]` is the packed bytecode (4-byte Instruction records).
//   - `constants` is indexed by the CONST opcode's inline argument.
//   - `inputs` holds one double per scalar input port for the current tuple.
//   - `state` is in/out; size must equal the operator's configured state size.
//   - `out_ptr` receives `num_outputs` doubles (one per END marker). Caller
//     guarantees capacity.
//
// Floating-point semantics: scalar libm for transcendentals; no fused-
// multiply-add reordering (caller's TU must be built with
// -ffp-contract=off -fno-associative-math for bit-exact parity guarantees).
inline void evaluate_one(
    const Instruction* ins,
    std::size_t ins_size,
    const double* constants,
    const double* inputs,
    double* state,
    double* out_ptr,
    std::size_t num_outputs) {
  double stack[64];
  std::size_t sp = 0;
  std::size_t out_idx = 0;

  for (std::size_t pc = 0; pc < ins_size; ++pc) {
    const Instruction i = ins[pc];
    switch (i.op) {
      case 0 /* INPUT */: {
        stack[sp++] = inputs[i.arg];
        break;
      }
      case 1 /* CONST */: {
        stack[sp++] = constants[i.arg];
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
        const std::uint16_t si = i.arg;
        double y = stack[--sp] - state[si + 1];
        double t = state[si] + y;
        state[si + 1] = (t - state[si]) - y;
        state[si] = t;
        stack[sp++] = state[si];
        break;
      }
      case 22 /* COUNT */: {
        const std::uint16_t si = i.arg;
        state[si] += 1.0;
        stack[sp++] = state[si];
        break;
      }
      case 23 /* MAX_AGG */: {
        const std::uint16_t si = i.arg;
        double v = stack[--sp];
        if (v > state[si]) state[si] = v;
        stack[sp++] = state[si];
        break;
      }
      case 24 /* MIN_AGG */: {
        const std::uint16_t si = i.arg;
        double v = stack[--sp];
        if (v < state[si]) state[si] = v;
        stack[sp++] = state[si];
        break;
      }
      case 25 /* STATE_LOAD */: {
        stack[sp++] = state[i.arg];
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
            "FusedScalarEval: unknown opcode " + std::to_string(i.op));
    }
  }

  (void)num_outputs;
  (void)out_idx;
}

}  // namespace rtbot::fuse

#endif  // RTBOT_FUSE_SCALAR_EVAL_H
