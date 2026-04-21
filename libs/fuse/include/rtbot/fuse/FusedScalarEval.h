#ifndef RTBOT_FUSE_SCALAR_EVAL_H
#define RTBOT_FUSE_SCALAR_EVAL_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedBytecode.h"

// NOTE: opcode numeric literals below correspond to the `rtbot::fused_op`
// namespace constants declared in FusedOps.h. Opcodes that use multi-field
// inline args (windowed/DSP ops like MA_UPDATE, FIR_UPDATE) read them from
// the AuxArgs side table; the Instruction::arg of those opcodes is an index
// into aux_args.

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
// Return value: true if the message should be emitted by the calling
// operator; false if any windowed opcode is still in warmup (e.g. MA_UPDATE
// before its first full window). Callers must skip output-message creation
// when false is returned — otherwise downstream timestamps diverge from an
// equivalent standalone-operator graph. All stateful side effects on `state`
// still happen during warmup so subsequent messages advance correctly; the
// writes to `out_ptr` are unspecified when false is returned.
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
inline bool evaluate_one(
    const Instruction* ins,
    std::size_t ins_size,
    const double* constants,
    const AuxArgs* aux_args,
    const double* coefficients,
    const double* inputs,
    double* state,
    double* out_ptr,
    std::size_t num_outputs) {
  (void)coefficients;  // consumed by FIR/IIR opcodes (added in a later step)
  bool emit = true;
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
      case 44 /* GATE */: {
        // Predicate gate: if zero, suppress the whole message's output.
        // Caller checks the returned emit flag and drops the output message.
        // Stack is rewound so subsequent expressions start fresh.
        const double pred = stack[--sp];
        if (pred == 0.0) emit = false;
        sp = 0;
        break;
      }
      case 38 /* DIFF */: {
        // State: [prev_value, has_prev]. Two-sample window — matches
        // standalone rtbot::Difference which suppresses the first message.
        const std::uint16_t off = i.arg;
        double& prev = state[off];
        double& has_prev = state[off + 1];
        const double v = stack[sp - 1];
        if (has_prev != 0.0) {
          stack[sp - 1] = v - prev;
        } else {
          emit = false;
          stack[sp - 1] = 0.0;
        }
        prev = v;
        has_prev = 1.0;
        break;
      }
      case 39 /* SIGN_CHANGE */: {
        // State: [prev_value, has_prev]. Emits sign(v - prev) on every
        // message after the first. No standalone counterpart; test against
        // a hand-coded reference.
        const std::uint16_t off = i.arg;
        double& prev = state[off];
        double& has_prev = state[off + 1];
        const double v = stack[sp - 1];
        if (has_prev != 0.0) {
          const double d = v - prev;
          stack[sp - 1] = (d > 0.0) ? 1.0 : (d < 0.0) ? -1.0 : 0.0;
        } else {
          emit = false;
          stack[sp - 1] = 0.0;
        }
        prev = v;
        has_prev = 1.0;
        break;
      }
      case 36 /* MSUM_UPDATE */: {
        // Identical update to MA_UPDATE but emits the running sum instead of
        // the running mean. Matches rtbot::MovingSum (emits this->sum() once
        // the window is full).
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        double* ring = &state[off];
        double& sum = state[off + W];
        double& comp = state[off + W + 1];
        double& count = state[off + W + 2];

        const double v = stack[sp - 1];
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
        if (count_i + 1 < W) emit = false;
        stack[sp - 1] = sum;
        break;
      }
      case 37 /* STD_UPDATE */: {
        // Matches rtbot::StandardDeviation: Kahan-compensated running sum over
        // the ring, then on each full-window message recompute M2 from
        // scratch via a second pass (2-pass variance), emit sqrt(M2 / (W-1)).
        // State layout: ring[0..W), [W]=sum, [W+1]=comp, [W+2]=count, [W+3]=_pad.
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        double* ring = &state[off];
        double& sum = state[off + W];
        double& comp = state[off + W + 1];
        double& count = state[off + W + 2];

        const double v = stack[sp - 1];
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
          emit = false;
          stack[sp - 1] = 0.0;
          break;
        }
        // Buffer is full — recompute M2 over the ring in the same deque-walk
        // order rtbot::Buffer uses (front-to-back, i.e. oldest-to-newest).
        // After this message's write, oldest sits at (idx + 1) % W when
        // count_i >= W; for count_i == W-1 the ring is filling front-to-back
        // at [0..W) and the natural walk matches.
        const double mean = sum / static_cast<double>(W);
        double m2 = 0.0;
        for (std::size_t k = 0; k < W; ++k) {
          std::size_t ring_idx;
          if (count_i + 1 == W) {
            ring_idx = k;  // ring filled [0..W), walk in order
          } else {
            ring_idx = (idx + 1 + k) % W;  // oldest first
          }
          const double d = ring[ring_idx] - mean;
          m2 += d * d;
        }
        stack[sp - 1] = std::sqrt(m2 / static_cast<double>(W - 1));
        break;
      }
      case 40 /* WIN_MIN */:
      case 41 /* WIN_MAX */: {
        // Monotonic deque (same algorithm as rtbot::WindowMinMax) over a
        // state-resident (pos, value) queue. State layout:
        //   [0]=pos (messages seen so far), [1]=deque_size,
        //   [2..W+2)=deque_values, [W+2..2W+2)=deque_positions.
        // pop_front implemented as O(W) memmove; W bounded and small, fine.
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        double& pos = state[off];
        double& size = state[off + 1];
        double* dq_vals = &state[off + 2];
        double* dq_pos = &state[off + 2 + W];
        const bool is_min = (i.op == 40);

        const double v = stack[sp - 1];

        // Pop back elements dominated by v.
        while (size > 0.0) {
          const std::size_t sz = static_cast<std::size_t>(size);
          const double back_v = dq_vals[sz - 1];
          const bool dominated = is_min ? (back_v >= v) : (back_v <= v);
          if (!dominated) break;
          size = size - 1.0;
        }
        // Push (pos, v).
        {
          const std::size_t sz = static_cast<std::size_t>(size);
          dq_vals[sz] = v;
          dq_pos[sz] = pos;
          size = size + 1.0;
        }
        // Evict front elements outside window [pos - W + 1, pos].
        // Matches standalone: `while (mono_.front().first + W <= pos_)`.
        while (size > 0.0 && dq_pos[0] + static_cast<double>(W) <= pos) {
          const std::size_t sz = static_cast<std::size_t>(size);
          for (std::size_t k = 1; k < sz; ++k) {
            dq_vals[k - 1] = dq_vals[k];
            dq_pos[k - 1] = dq_pos[k];
          }
          size = size - 1.0;
        }

        // Emit front value iff pos >= W - 1.
        if (pos + 1.0 >= static_cast<double>(W)) {
          stack[sp - 1] = dq_vals[0];
        } else {
          emit = false;
          stack[sp - 1] = 0.0;
        }
        pos = pos + 1.0;
        break;
      }
      case 42 /* FIR_UPDATE */: {
        // Matches rtbot::FiniteImpulseResponse: once the ring has W values,
        // emit sum_{i=0..W-1} coeff[i] * (i-th newest value). Coefficient
        // iteration order is left-to-right (coeff[0] first), matching the
        // standalone's for-loop. State layout: ring[0..W), [W]=head (next
        // write position), [W+1]=count.
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        const std::size_t coeff_off = a.c;
        double* ring = &state[off];
        double& head = state[off + W];
        double& count = state[off + W + 1];

        const double v = stack[sp - 1];
        const std::size_t idx = static_cast<std::size_t>(head);
        ring[idx] = v;
        head = static_cast<double>((idx + 1) % W);
        count = count + 1.0;

        if (count < static_cast<double>(W)) {
          emit = false;
          stack[sp - 1] = 0.0;
          break;
        }
        double result = 0.0;
        for (std::size_t k = 0; k < W; ++k) {
          const std::size_t ring_idx = (idx + W - k) % W;
          result += coefficients[coeff_off + k] * ring[ring_idx];
        }
        stack[sp - 1] = result;
        break;
      }
      case 43 /* IIR_UPDATE */: {
        // Matches rtbot::InfiniteImpulseResponse. AuxArgs:
        //   {state_off, b_len, a_len, coeff_off}
        // coefficients[coeff_off..coeff_off+b_len) = b_coeffs
        // coefficients[coeff_off+b_len..coeff_off+b_len+a_len) = a_coeffs
        // State layout:
        //   [0]=x_head, [1]=x_count, [2]=y_head, [3]=y_count,
        //   [4..4+b_len)=x_ring, [4+b_len..4+b_len+a_len)=y_ring.
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

        const double v = stack[sp - 1];
        const std::size_t xi = static_cast<std::size_t>(x_head);
        x_ring[xi] = v;
        x_head = static_cast<double>((xi + 1) % B_len);
        if (x_count < static_cast<double>(B_len)) x_count = x_count + 1.0;

        if (x_count < static_cast<double>(B_len)) {
          emit = false;
          stack[sp - 1] = 0.0;
          break;
        }
        // Kahan-compensated: y_n = sum b[i] * x[i-th newest] - sum a[i] * y[i-th newest]
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
        if (A_len > 0) {
          const std::size_t yi = static_cast<std::size_t>(y_head);
          y_ring[yi] = y_n;
          y_head = static_cast<double>((yi + 1) % A_len);
          if (y_count < static_cast<double>(A_len)) y_count = y_count + 1.0;
        }

        stack[sp - 1] = y_n;
        break;
      }
      case 35 /* MA_UPDATE */: {
        // aux_args[i.arg] = {state_off, win_size, _, _}.
        // state layout: ring[0..W) then [W]=kahan_sum, [W+1]=kahan_comp,
        // [W+2]=count. Matches rtbot::Buffer's kahan_subtract-then-kahan_add
        // order so parity against standalone MovingAverage is bit-exact once
        // the window fills (fused opcode still emits a running mean during
        // warmup; standalone suppresses those messages entirely).
        const AuxArgs& a = aux_args[i.arg];
        const std::size_t off = a.a;
        const std::size_t W = a.b;
        double* ring = &state[off];
        double& sum = state[off + W];
        double& comp = state[off + W + 1];
        double& count = state[off + W + 2];

        const double v = stack[sp - 1];
        const std::size_t count_i = static_cast<std::size_t>(count);
        const std::size_t idx = count_i % W;

        if (count_i >= W) {
          // kahan_subtract(leaving) = kahan_add(-leaving)
          const double leaving = ring[idx];
          const double ys = (-leaving) - comp;
          const double ts = sum + ys;
          comp = (ts - sum) - ys;
          sum = ts;
        }
        ring[idx] = v;
        // kahan_add(v)
        const double ya = v - comp;
        const double ta = sum + ya;
        comp = (ta - sum) - ya;
        sum = ta;

        count = count + 1.0;
        // Suppress emission until the window is full — matches standalone
        // MovingAverage::process_message which returns {} on partial window.
        if (count_i + 1 < W) emit = false;
        const double n = (count_i + 1 >= W) ? static_cast<double>(W)
                                             : static_cast<double>(count_i + 1);
        stack[sp - 1] = sum / n;
        break;
      }
      default:
        throw std::runtime_error(
            "FusedScalarEval: unknown opcode " + std::to_string(i.op));
    }
  }

  (void)num_outputs;
  (void)out_idx;
  return emit;
}

}  // namespace rtbot::fuse

#endif  // RTBOT_FUSE_SCALAR_EVAL_H
