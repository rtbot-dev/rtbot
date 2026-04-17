#ifndef RTBOT_FUSE_BATCH_EVAL_H
#define RTBOT_FUSE_BATCH_EVAL_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"

// RTBOT_FUSED_SIMD_ENABLED is the authoritative compile-time switch used by
// the evaluator cases below. It's derived from RTBOT_FUSED_SIMD (the user-
// facing flag) with a carveout for Emscripten/WASM builds — xsimd's WASM
// arch requires -msimd128, which isn't on by default in the rtbot Emscripten
// toolchain. Once WASM SIMD is wired through the build system this guard can
// be lifted.
#if defined(RTBOT_FUSED_SIMD) && !defined(__EMSCRIPTEN__)
#define RTBOT_FUSED_SIMD_ENABLED 1
#include "rtbot/fuse/FusedSimdOps.h"
#endif

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
//   lane_emit[l]     = in/out; caller initializes all to true, windowed
//                      opcodes clear individual lanes whose window is still
//                      in warmup. Caller must skip emitting messages for
//                      lanes where lane_emit[l] is false.
template <std::size_t B>
inline void evaluate_batched(
    const Instruction* ins,
    std::size_t ins_size,
    const double* constants,
    const AuxArgs* aux_args,
    const double* coefficients,
    const std::array<double, B>* inputs,
    std::size_t active_lanes,
    double* state,
    double* out,
    std::size_t num_outputs,
    std::array<bool, B>& lane_emit) {
  (void)aux_args;      // consumed by tier-1 windowed opcodes (phase 3)
  (void)coefficients;  // consumed by FIR/IIR opcodes (phase 3)
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
#if defined(RTBOT_FUSED_SIMD_ENABLED)
        rtbot::fuse::simd::add_lanes<B>(stack[sp - 1], stack[sp], active_lanes);
#else
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] += stack[sp][l];
#endif
        break;
      }
      case 3 /* SUB */: {
        --sp;
#if defined(RTBOT_FUSED_SIMD_ENABLED)
        rtbot::fuse::simd::sub_lanes<B>(stack[sp - 1], stack[sp], active_lanes);
#else
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] -= stack[sp][l];
#endif
        break;
      }
      case 4 /* MUL */: {
        --sp;
#if defined(RTBOT_FUSED_SIMD_ENABLED)
        rtbot::fuse::simd::mul_lanes<B>(stack[sp - 1], stack[sp], active_lanes);
#else
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] *= stack[sp][l];
#endif
        break;
      }
      case 5 /* DIV */: {
        --sp;
#if defined(RTBOT_FUSED_SIMD_ENABLED)
        rtbot::fuse::simd::div_lanes<B>(stack[sp - 1], stack[sp], active_lanes);
#else
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] /= stack[sp][l];
#endif
        break;
      }
      case 6 /* POW */: {
        --sp;
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::pow(stack[sp - 1][l], stack[sp][l]);
        break;
      }
      case 7 /* ABS */: {
#if defined(RTBOT_FUSED_SIMD_ENABLED)
        rtbot::fuse::simd::abs_lanes<B>(stack[sp - 1], active_lanes);
#else
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = std::abs(stack[sp - 1][l]);
#endif
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
#if defined(RTBOT_FUSED_SIMD_ENABLED)
        rtbot::fuse::simd::neg_lanes<B>(stack[sp - 1], active_lanes);
#else
        for (std::size_t l = 0; l < active_lanes; ++l)
          stack[sp - 1][l] = -stack[sp - 1][l];
#endif
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
      // Tier-1 windowed opcodes. All run serially per lane since they mutate
      // shared state; each lane reads post-update state matching its position
      // in the message sequence. lane_emit is cleared for lanes whose window
      // is still in warmup, matching rtbot::Buffer-based operators that
      // suppress output until buffer_full().
      case 35 /* MA_UPDATE */: {
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        double* ring = &state[off];
        double& sum = state[off + W];
        double& comp = state[off + W + 1];
        double& count = state[off + W + 2];
        for (std::size_t l = 0; l < active_lanes; ++l) {
          const double v = stack[sp - 1][l];
          const std::size_t count_i = static_cast<std::size_t>(count);
          const std::size_t idx = count_i % W;
          if (count_i >= W) {
            const double leaving = ring[idx];
            const double ys = (-leaving) - comp;
            const double ts = sum + ys;
            comp = (ts - sum) - ys;
            sum = ts;
          }
          ring[idx] = v;
          const double ya = v - comp;
          const double ta = sum + ya;
          comp = (ta - sum) - ya;
          sum = ta;
          count = count + 1.0;
          if (count_i + 1 < W) lane_emit[l] = false;
          const double n = (count_i + 1 >= W)
                                ? static_cast<double>(W)
                                : static_cast<double>(count_i + 1);
          stack[sp - 1][l] = sum / n;
        }
        break;
      }
      case 36 /* MSUM_UPDATE */: {
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        double* ring = &state[off];
        double& sum = state[off + W];
        double& comp = state[off + W + 1];
        double& count = state[off + W + 2];
        for (std::size_t l = 0; l < active_lanes; ++l) {
          const double v = stack[sp - 1][l];
          const std::size_t count_i = static_cast<std::size_t>(count);
          const std::size_t idx = count_i % W;
          if (count_i >= W) {
            const double leaving = ring[idx];
            const double ys = (-leaving) - comp;
            const double ts = sum + ys;
            comp = (ts - sum) - ys;
            sum = ts;
          }
          ring[idx] = v;
          const double ya = v - comp;
          const double ta = sum + ya;
          comp = (ta - sum) - ya;
          sum = ta;
          count = count + 1.0;
          if (count_i + 1 < W) lane_emit[l] = false;
          stack[sp - 1][l] = sum;
        }
        break;
      }
      case 37 /* STD_UPDATE */: {
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        double* ring = &state[off];
        double& sum = state[off + W];
        double& comp = state[off + W + 1];
        double& count = state[off + W + 2];
        for (std::size_t l = 0; l < active_lanes; ++l) {
          const double v = stack[sp - 1][l];
          const std::size_t count_i = static_cast<std::size_t>(count);
          const std::size_t idx = count_i % W;
          if (count_i >= W) {
            const double leaving = ring[idx];
            const double ys = (-leaving) - comp;
            const double ts = sum + ys;
            comp = (ts - sum) - ys;
            sum = ts;
          }
          ring[idx] = v;
          const double ya = v - comp;
          const double ta = sum + ya;
          comp = (ta - sum) - ya;
          sum = ta;
          count = count + 1.0;
          if (count_i + 1 < W) {
            lane_emit[l] = false;
            stack[sp - 1][l] = 0.0;
            continue;
          }
          const double mean = sum / static_cast<double>(W);
          double m2 = 0.0;
          for (std::size_t k = 0; k < W; ++k) {
            const std::size_t ring_idx =
                (count_i + 1 == W) ? k : (idx + 1 + k) % W;
            const double d = ring[ring_idx] - mean;
            m2 += d * d;
          }
          stack[sp - 1][l] = std::sqrt(m2 / static_cast<double>(W - 1));
        }
        break;
      }
      case 38 /* DIFF */: {
        const std::uint16_t off = i.arg;
        double& prev = state[off];
        double& has_prev = state[off + 1];
        for (std::size_t l = 0; l < active_lanes; ++l) {
          const double v = stack[sp - 1][l];
          if (has_prev != 0.0) {
            stack[sp - 1][l] = v - prev;
          } else {
            lane_emit[l] = false;
            stack[sp - 1][l] = 0.0;
          }
          prev = v;
          has_prev = 1.0;
        }
        break;
      }
      case 39 /* SIGN_CHANGE */: {
        const std::uint16_t off = i.arg;
        double& prev = state[off];
        double& has_prev = state[off + 1];
        for (std::size_t l = 0; l < active_lanes; ++l) {
          const double v = stack[sp - 1][l];
          if (has_prev != 0.0) {
            const double d = v - prev;
            stack[sp - 1][l] =
                (d > 0.0) ? 1.0 : (d < 0.0) ? -1.0 : 0.0;
          } else {
            lane_emit[l] = false;
            stack[sp - 1][l] = 0.0;
          }
          prev = v;
          has_prev = 1.0;
        }
        break;
      }
      case 40 /* WIN_MIN */:
      case 41 /* WIN_MAX */: {
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        double& pos = state[off];
        double& size = state[off + 1];
        double* dq_vals = &state[off + 2];
        double* dq_pos = &state[off + 2 + W];
        const bool is_min = (i.op == 40);
        for (std::size_t l = 0; l < active_lanes; ++l) {
          const double v = stack[sp - 1][l];
          while (size > 0.0) {
            const std::size_t sz = static_cast<std::size_t>(size);
            const double back_v = dq_vals[sz - 1];
            const bool dominated = is_min ? (back_v >= v) : (back_v <= v);
            if (!dominated) break;
            size = size - 1.0;
          }
          {
            const std::size_t sz = static_cast<std::size_t>(size);
            dq_vals[sz] = v;
            dq_pos[sz] = pos;
            size = size + 1.0;
          }
          while (size > 0.0 && dq_pos[0] + static_cast<double>(W) <= pos) {
            const std::size_t sz = static_cast<std::size_t>(size);
            for (std::size_t k = 1; k < sz; ++k) {
              dq_vals[k - 1] = dq_vals[k];
              dq_pos[k - 1] = dq_pos[k];
            }
            size = size - 1.0;
          }
          if (pos + 1.0 >= static_cast<double>(W)) {
            stack[sp - 1][l] = dq_vals[0];
          } else {
            lane_emit[l] = false;
            stack[sp - 1][l] = 0.0;
          }
          pos = pos + 1.0;
        }
        break;
      }
      case 42 /* FIR_UPDATE */: {
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        const std::size_t coeff_off = a.c;
        double* ring = &state[off];
        double& head = state[off + W];
        double& count = state[off + W + 1];
        for (std::size_t l = 0; l < active_lanes; ++l) {
          const double v = stack[sp - 1][l];
          const std::size_t idx = static_cast<std::size_t>(head);
          ring[idx] = v;
          head = static_cast<double>((idx + 1) % W);
          count = count + 1.0;
          if (count < static_cast<double>(W)) {
            lane_emit[l] = false;
            stack[sp - 1][l] = 0.0;
            continue;
          }
          double result = 0.0;
          for (std::size_t k = 0; k < W; ++k) {
            const std::size_t ring_idx = (idx + W - k) % W;
            result += coefficients[coeff_off + k] * ring[ring_idx];
          }
          stack[sp - 1][l] = result;
        }
        break;
      }
      case 43 /* IIR_UPDATE */: {
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t B_len = a.b;
        const std::size_t A_len = a.c;
        const std::size_t c_off = a.d;
        double& x_head = state[off];
        double& x_count = state[off + 1];
        double& y_head = state[off + 2];
        double& y_count = state[off + 3];
        double* x_ring = &state[off + 4];
        double* y_ring = &state[off + 4 + B_len];
        const double* b_coefs = &coefficients[c_off];
        const double* a_coefs = &coefficients[c_off + B_len];
        for (std::size_t l = 0; l < active_lanes; ++l) {
          const double v = stack[sp - 1][l];
          const std::size_t xi = static_cast<std::size_t>(x_head);
          x_ring[xi] = v;
          x_head = static_cast<double>((xi + 1) % B_len);
          if (x_count < static_cast<double>(B_len)) x_count = x_count + 1.0;
          if (x_count < static_cast<double>(B_len)) {
            lane_emit[l] = false;
            stack[sp - 1][l] = 0.0;
            continue;
          }
          double y_n = 0.0, y_comp = 0.0;
          for (std::size_t k = 0; k < B_len; ++k) {
            const std::size_t ri = (xi + B_len - k) % B_len;
            const double term = b_coefs[k] * x_ring[ri] - y_comp;
            const double t = y_n + term;
            y_comp = (t - y_n) - term;
            y_n = t;
          }
          const std::size_t y_avail = static_cast<std::size_t>(y_count);
          const std::size_t y_use = std::min(y_avail, A_len);
          for (std::size_t k = 0; k < y_use; ++k) {
            const std::size_t yi_back =
                (static_cast<std::size_t>(y_head) + A_len - 1 - k) % A_len;
            const double term = -(a_coefs[k] * y_ring[yi_back]) - y_comp;
            const double t = y_n + term;
            y_comp = (t - y_n) - term;
            y_n = t;
          }
          const std::size_t yi = static_cast<std::size_t>(y_head);
          y_ring[yi] = y_n;
          y_head = static_cast<double>((yi + 1) % A_len);
          if (y_count < static_cast<double>(A_len)) y_count = y_count + 1.0;
          stack[sp - 1][l] = y_n;
        }
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
