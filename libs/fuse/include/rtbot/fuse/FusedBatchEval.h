#ifndef RTBOT_FUSE_BATCH_EVAL_H
#define RTBOT_FUSE_BATCH_EVAL_H

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"

namespace rtbot::fuse {

// Batch width for lane-parallel evaluation. The compiler autovectorizes
// the inner `for (l = 0; l < B; ++l)` loops in evaluate_batched to AVX2
// (4 lanes), NEON (2 lanes), or WASM SIMD128 (2 lanes) without any SIMD
// library dependency.
//
// Native builds use B=8 so the compiler can unroll to two SIMD ops per
// opcode. WASM uses B=4 since Emscripten's SIMD128 is narrower and the
// larger stack frame hurts more there.
#if defined(__EMSCRIPTEN__)
inline constexpr std::size_t kBatch = 4;
#else
inline constexpr std::size_t kBatch = 8;
#endif

// evaluate_batched<B> — lane-parallel scalar RPN evaluator.
//
// Runs the same packed bytecode across up to B synchronized input tuples in
// one dispatch pass. Pure arithmetic, comparisons, booleans, and unary math
// become inner loops over B — the compiler autovectorizes them.
//
// Stateful opcodes (CUMSUM, COUNT, MAX_AGG, MIN_AGG, STATE_LOAD) are processed
// serially in lane order inside the batched loop. This preserves Kahan
// compensation sequencing and message-order semantics bit-exactly against
// the scalar reference.
//
// Transcendentals (SQRT, LOG, EXP, SIN, COS, TAN, POW, LOG10) call scalar
// libm once per lane. SIMD transcendentals land in Phase 5 behind a
// ULP-tolerance flag.
//
// Contract:
//   inputs[p][l]     = value on port p for lane l (l in [0, active_lanes))
//                      caller passes an array-of-arrays pointer; stored
//                      typically on the caller's stack to avoid per-dispatch
//                      heap allocation
//   active_lanes     = number of valid lanes, 1..B
//   state            = in/out, shared across lanes; stateful opcodes mutate
//                      it once per lane in lane order
//   out[l*num_outputs + k] = output k for lane l
template <std::size_t B>
inline void evaluate_batched(
    const Instruction* ins,
    std::size_t ins_size,
    const double* constants,
    const std::array<double, B>* inputs,
    std::size_t active_lanes,
    double* state,
    double* out,
    std::size_t num_outputs) {
  std::array<std::array<double, B>, 64> stack;
  std::size_t sp = 0;
  std::array<std::size_t, B> out_idx{};

  for (std::size_t pc = 0; pc < ins_size; ++pc) {
    const Instruction i = ins[pc];
    switch (i.op) {
      case 0 /* INPUT */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp][l] = inputs[i.arg][l];
        ++sp;
        break;
      }
      case 1 /* CONST */: {
        const double c = constants[i.arg];
        for (std::size_t l = 0; l < active_lanes; ++l) stack[sp][l] = c;
        ++sp;
        break;
      }
      case 2 /* ADD */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] += stack[sp][l];
        break;
      }
      case 3 /* SUB */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] -= stack[sp][l];
        break;
      }
      case 4 /* MUL */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] *= stack[sp][l];
        break;
      }
      case 5 /* DIV */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] /= stack[sp][l];
        break;
      }
      case 6 /* POW */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::pow(stack[sp - 1][l], stack[sp][l]);
        break;
      }
      case 7 /* ABS */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::abs(stack[sp - 1][l]);
        break;
      }
      case 8 /* SQRT */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::sqrt(stack[sp - 1][l]);
        break;
      }
      case 9 /* LOG */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::log(stack[sp - 1][l]);
        break;
      }
      case 10 /* LOG10 */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::log10(stack[sp - 1][l]);
        break;
      }
      case 11 /* EXP */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::exp(stack[sp - 1][l]);
        break;
      }
      case 12 /* SIN */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::sin(stack[sp - 1][l]);
        break;
      }
      case 13 /* COS */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::cos(stack[sp - 1][l]);
        break;
      }
      case 14 /* TAN */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::tan(stack[sp - 1][l]);
        break;
      }
      case 15 /* SIGN */: {
        for (std::size_t l = 0; l < active_lanes; ++l) {
          double v = stack[sp - 1][l];
          stack[sp - 1][l] = (v > 0.0) ? 1.0 : (v < 0.0) ? -1.0 : 0.0;
        }
        break;
      }
      case 16 /* FLOOR */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::floor(stack[sp - 1][l]);
        break;
      }
      case 17 /* CEIL */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::ceil(stack[sp - 1][l]);
        break;
      }
      case 18 /* ROUND */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::round(stack[sp - 1][l]);
        break;
      }
      case 19 /* NEG */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = -stack[sp - 1][l];
        break;
      }
      case 20 /* END */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l) {
          out[l * num_outputs + out_idx[l]++] = stack[sp][l];
        }
        sp = 0;
        break;
      }
      case 21 /* CUMSUM */: {
        const std::uint16_t si = i.arg;
        for (std::size_t l = 0; l < active_lanes; ++l) {
          double y = stack[sp - 1][l] - state[si + 1];
          double t = state[si] + y;
          state[si + 1] = (t - state[si]) - y;
          state[si] = t;
          stack[sp - 1][l] = state[si];
        }
        break;
      }
      case 22 /* COUNT */: {
        const std::uint16_t si = i.arg;
        for (std::size_t l = 0; l < active_lanes; ++l) {
          state[si] += 1.0;
          stack[sp][l] = state[si];
        }
        ++sp;
        break;
      }
      case 23 /* MAX_AGG */: {
        const std::uint16_t si = i.arg;
        for (std::size_t l = 0; l < active_lanes; ++l) {
          double v = stack[sp - 1][l];
          if (v > state[si]) state[si] = v;
          stack[sp - 1][l] = state[si];
        }
        break;
      }
      case 24 /* MIN_AGG */: {
        const std::uint16_t si = i.arg;
        for (std::size_t l = 0; l < active_lanes; ++l) {
          double v = stack[sp - 1][l];
          if (v < state[si]) state[si] = v;
          stack[sp - 1][l] = state[si];
        }
        break;
      }
      case 25 /* STATE_LOAD */: {
        const double v = state[i.arg];
        for (std::size_t l = 0; l < active_lanes; ++l) stack[sp][l] = v;
        ++sp;
        break;
      }
      case 26 /* GT */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = (stack[sp - 1][l] > stack[sp][l]) ? 1.0 : 0.0;
        break;
      }
      case 27 /* GTE */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = (stack[sp - 1][l] >= stack[sp][l]) ? 1.0 : 0.0;
        break;
      }
      case 28 /* LT */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = (stack[sp - 1][l] < stack[sp][l]) ? 1.0 : 0.0;
        break;
      }
      case 29 /* LTE */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = (stack[sp - 1][l] <= stack[sp][l]) ? 1.0 : 0.0;
        break;
      }
      case 30 /* EQ */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = (stack[sp - 1][l] == stack[sp][l]) ? 1.0 : 0.0;
        break;
      }
      case 31 /* NEQ */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = (stack[sp - 1][l] != stack[sp][l]) ? 1.0 : 0.0;
        break;
      }
      case 32 /* AND */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] =
              (stack[sp - 1][l] != 0.0 && stack[sp][l] != 0.0) ? 1.0 : 0.0;
        break;
      }
      case 33 /* OR */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] =
              (stack[sp - 1][l] != 0.0 || stack[sp][l] != 0.0) ? 1.0 : 0.0;
        break;
      }
      case 34 /* NOT */: {
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = (stack[sp - 1][l] == 0.0) ? 1.0 : 0.0;
        break;
      }
      default:
        throw std::runtime_error(
            "evaluate_batched: unknown opcode " + std::to_string(i.op));
    }
  }

  (void)num_outputs;
}

}  // namespace rtbot::fuse

#endif  // RTBOT_FUSE_BATCH_EVAL_H
