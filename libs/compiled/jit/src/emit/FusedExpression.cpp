// FusedExpression.cpp
//
// IR emission for the FusedExpression operator. Walks the FE RPN bytecode
// and produces an SSA value for every END marker, plus a single emit_flag
// that ANDs together all per-opcode suppression conditions (windowed
// warmup, DIFF first-sample, GATE predicate, etc.).
//
// Reuses the existing per-opcode IR helpers wherever the FE state shape
// and the JIT helper agree. DIFF is the lone exception — FE uses a 2-slot
// state ([prev, has_prev]) while emit_diff in the JIT uses 4 — so DIFF is
// inlined here directly to mirror FusedScalarEval case 38 exactly.

#include "rtbot/compiled/jit/emit/FusedExpression.h"

#include <stdexcept>
#include <string>
#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

#include "rtbot/compiled/jit/emit/Aggregate.h"
#include "rtbot/compiled/jit/emit/Arithmetic.h"
#include "rtbot/compiled/jit/emit/Boolean.h"
#include "rtbot/compiled/jit/emit/Comparison.h"
#include "rtbot/compiled/jit/emit/FIR.h"
#include "rtbot/compiled/jit/emit/IIR.h"
#include "rtbot/compiled/jit/emit/MovingAverage.h"
#include "rtbot/compiled/jit/emit/MovingSum.h"
#include "rtbot/compiled/jit/emit/SignChange.h"
#include "rtbot/compiled/jit/emit/StateLoad.h"
#include "rtbot/compiled/jit/emit/StdDev.h"
#include "rtbot/compiled/jit/emit/Transcendental.h"
#include "rtbot/compiled/jit/emit/WindowMinMax.h"
#include "rtbot/fuse/FusedAuxArgs.h"
#include "rtbot/fuse/FusedBytecode.h"
#include "rtbot/fuse/FusedOps.h"

namespace rtbot::jit::emit {

namespace {

// Inline DIFF emit matching FE state layout [prev, has_prev] — distinct from
// the standalone JIT emit_diff which uses 4 slots.
//
// Mirrors FusedScalarEval case 38:
//   if (has_prev != 0.0)  out = v - prev;  emit ok
//   else                  out = 0.0;       emit_flag = false
//   prev = v; has_prev = 1.0;
struct FEDiffOut {
  llvm::Value* out_v;     // double
  llvm::Value* emit_flag; // i1
};

FEDiffOut emit_fe_diff(IrEmissionContext& ec, std::size_t state_offset,
                       llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);

  llvm::Value* prev_ptr     = ec.state_gep(state_offset + 0);
  llvm::Value* has_prev_ptr = ec.state_gep(state_offset + 1);

  llvm::Value* prev = b.CreateLoad(f64, prev_ptr, "fed_prev");
  llvm::Value* hp   = b.CreateLoad(f64, has_prev_ptr, "fed_hp");
  llvm::Value* has  = b.CreateFCmpONE(hp, llvm::ConstantFP::get(f64, 0.0),
                                       "fed_has");
  llvm::Value* dv   = b.CreateFSub(v, prev, "fed_d");
  llvm::Value* outv = b.CreateSelect(has, dv,
                                      llvm::ConstantFP::get(f64, 0.0),
                                      "fed_out");
  // Advance state unconditionally.
  b.CreateStore(v, prev_ptr);
  b.CreateStore(llvm::ConstantFP::get(f64, 1.0), has_prev_ptr);
  return {outv, has};
}

// Pop helper.
llvm::Value* pop(std::vector<llvm::Value*>& stack) {
  llvm::Value* v = stack.back();
  stack.pop_back();
  return v;
}

}  // namespace

FusedExprOutput emit_fused_expression(
    IrEmissionContext& ec,
    std::size_t state_offset,
    const std::vector<double>& bytecode,
    const std::vector<double>& constants,
    const std::vector<double>& coefficients,
    const std::vector<llvm::Value*>& inputs,
    std::size_t num_outputs) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();
  llvm::Type* f64 = llvm::Type::getDoubleTy(ctx);

  auto cf = [&](double val) -> llvm::Value* {
    return llvm::ConstantFP::get(f64, val);
  };

  // Pack the public bytecode the same way FusedExpression does at construction.
  // This produces packed instructions and the aux_args side table that
  // describes per-opcode state offsets and window sizes for windowed ops.
  auto pack = rtbot::fuse::pack_bytecode(bytecode);

  // Stack of SSA double values (matching FE's runtime double stack).
  std::vector<llvm::Value*> stack;
  stack.reserve(64);

  // Per-END output values.
  std::vector<llvm::Value*> out_vs;
  out_vs.reserve(num_outputs);

  // Combined emit flag — ANDed across every opcode that may suppress emission.
  llvm::Value* emit_flag = llvm::ConstantInt::getTrue(ctx);

  // Time argument is not an input to bytecode opcodes — DIFF / SIGN_CHANGE
  // call sites pass an i64 zero placeholder when only the value is needed.

  // Token used as `t` for opcodes that take a timestamp arg but where the
  // FE evaluator does not actually use it. The JIT helpers' StatefulOutput
  // returns out_t but in FE bytecode flow the output time of the FE itself
  // comes from the synced port queue — never from these inner ops. So we
  // can pass any i64 here; we use the constant 0.
  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);
  llvm::Value* dummy_t = llvm::ConstantInt::get(i64, 0);

  // The bytecode offset for stateful opcodes is the offset INTO the FE's
  // bytecode-state region (state_offset). We add state_offset to get the
  // absolute offset into the program-wide state buffer.
  auto abs = [&](std::size_t off) {
    return state_offset + off;
  };

  for (const rtbot::fuse::Instruction& ins : pack.packed) {
    const std::uint8_t op = ins.op;
    const std::uint16_t arg = ins.arg;

    switch (op) {
      case 0 /* INPUT */: {
        if (arg >= inputs.size()) {
          throw std::runtime_error(
              "emit_fused_expression: INPUT idx " + std::to_string(arg) +
              " out of range (numPorts=" + std::to_string(inputs.size()) + ")");
        }
        stack.push_back(inputs[arg]);
        break;
      }
      case 1 /* CONST */: {
        if (arg >= constants.size()) {
          throw std::runtime_error(
              "emit_fused_expression: CONST idx " + std::to_string(arg) +
              " out of range (constants size=" +
              std::to_string(constants.size()) + ")");
        }
        stack.push_back(cf(constants[arg]));
        break;
      }
      case 2 /* ADD */: {
        llvm::Value* y = pop(stack);
        llvm::Value* x = pop(stack);
        stack.push_back(emit_add(ec, x, y));
        break;
      }
      case 3 /* SUB */: {
        llvm::Value* y = pop(stack);
        llvm::Value* x = pop(stack);
        stack.push_back(emit_sub(ec, x, y));
        break;
      }
      case 4 /* MUL */: {
        llvm::Value* y = pop(stack);
        llvm::Value* x = pop(stack);
        stack.push_back(emit_mul(ec, x, y));
        break;
      }
      case 5 /* DIV */: {
        llvm::Value* y = pop(stack);
        llvm::Value* x = pop(stack);
        stack.push_back(emit_div(ec, x, y));
        break;
      }
      case 6 /* POW */: {
        llvm::Value* y = pop(stack);
        llvm::Value* x = pop(stack);
        stack.push_back(emit_pow(ec, x, y));
        break;
      }
      case 7 /* ABS */:   { llvm::Value* x = pop(stack); stack.push_back(emit_abs  (ec, x)); break; }
      case 8 /* SQRT */:  { llvm::Value* x = pop(stack); stack.push_back(emit_sqrt (ec, x)); break; }
      case 9 /* LOG */:   { llvm::Value* x = pop(stack); stack.push_back(emit_log  (ec, x)); break; }
      case 10/* LOG10 */: { llvm::Value* x = pop(stack); stack.push_back(emit_log10(ec, x)); break; }
      case 11/* EXP */:   { llvm::Value* x = pop(stack); stack.push_back(emit_exp  (ec, x)); break; }
      case 12/* SIN */:   { llvm::Value* x = pop(stack); stack.push_back(emit_sin  (ec, x)); break; }
      case 13/* COS */:   { llvm::Value* x = pop(stack); stack.push_back(emit_cos  (ec, x)); break; }
      case 14/* TAN */:   { llvm::Value* x = pop(stack); stack.push_back(emit_tan  (ec, x)); break; }
      case 15/* SIGN */:  { llvm::Value* x = pop(stack); stack.push_back(emit_sign (ec, x)); break; }
      case 16/* FLOOR */: { llvm::Value* x = pop(stack); stack.push_back(emit_floor(ec, x)); break; }
      case 17/* CEIL */:  { llvm::Value* x = pop(stack); stack.push_back(emit_ceil (ec, x)); break; }
      case 18/* ROUND */: { llvm::Value* x = pop(stack); stack.push_back(emit_round(ec, x)); break; }
      case 19/* NEG */:   { llvm::Value* x = pop(stack); stack.push_back(emit_neg  (ec, x)); break; }

      case 20 /* END */: {
        if (stack.empty()) {
          throw std::runtime_error(
              "emit_fused_expression: END with empty stack");
        }
        out_vs.push_back(stack.back());
        stack.clear();
        break;
      }

      case 21 /* CUMSUM */: {
        // Reuse Aggregate.cpp Kahan helper — same 2-slot layout (sum, comp).
        llvm::Value* x   = pop(stack);
        llvm::Value* res = emit_cumsum(ec, abs(arg), x);
        stack.push_back(res);
        break;
      }
      case 22 /* COUNT */: {
        // Same single-slot layout.
        llvm::Value* res = emit_count(ec, abs(arg));
        stack.push_back(res);
        break;
      }
      case 23 /* MAX_AGG */: {
        llvm::Value* x   = pop(stack);
        llvm::Value* res = emit_max_agg(ec, abs(arg), x);
        stack.push_back(res);
        break;
      }
      case 24 /* MIN_AGG */: {
        llvm::Value* x   = pop(stack);
        llvm::Value* res = emit_min_agg(ec, abs(arg), x);
        stack.push_back(res);
        break;
      }
      case 25 /* STATE_LOAD */: {
        // arg here is an absolute slot offset within the FE state region.
        llvm::Value* res = emit_state_load(ec, abs(arg));
        stack.push_back(res);
        break;
      }

      case 26 /* GT */: {
        llvm::Value* y = pop(stack); llvm::Value* x = pop(stack);
        stack.push_back(emit_gt(ec, x, y)); break;
      }
      case 27 /* GTE */: {
        llvm::Value* y = pop(stack); llvm::Value* x = pop(stack);
        stack.push_back(emit_gte(ec, x, y)); break;
      }
      case 28 /* LT */: {
        llvm::Value* y = pop(stack); llvm::Value* x = pop(stack);
        stack.push_back(emit_lt(ec, x, y)); break;
      }
      case 29 /* LTE */: {
        llvm::Value* y = pop(stack); llvm::Value* x = pop(stack);
        stack.push_back(emit_lte(ec, x, y)); break;
      }
      case 30 /* EQ */: {
        llvm::Value* y = pop(stack); llvm::Value* x = pop(stack);
        stack.push_back(emit_eq(ec, x, y)); break;
      }
      case 31 /* NEQ */: {
        llvm::Value* y = pop(stack); llvm::Value* x = pop(stack);
        stack.push_back(emit_neq(ec, x, y)); break;
      }
      case 32 /* AND */: {
        llvm::Value* y = pop(stack); llvm::Value* x = pop(stack);
        stack.push_back(emit_and(ec, x, y)); break;
      }
      case 33 /* OR */: {
        llvm::Value* y = pop(stack); llvm::Value* x = pop(stack);
        stack.push_back(emit_or(ec, x, y)); break;
      }
      case 34 /* NOT */: {
        llvm::Value* x = pop(stack);
        stack.push_back(emit_not(ec, x));
        break;
      }

      case 35 /* MA_UPDATE */: {
        const rtbot::fuse::AuxArgs& a = pack.aux_args[arg];
        llvm::Value* x = pop(stack);
        StatefulOutput so = emit_moving_average(ec, abs(a.a), a.b, dummy_t, x);
        emit_flag = b.CreateAnd(emit_flag, so.emit_flag, "fe_ef_ma");
        stack.push_back(so.out_v);
        break;
      }
      case 36 /* MSUM_UPDATE */: {
        const rtbot::fuse::AuxArgs& a = pack.aux_args[arg];
        llvm::Value* x = pop(stack);
        StatefulOutput so = emit_moving_sum(ec, abs(a.a), a.b, dummy_t, x);
        emit_flag = b.CreateAnd(emit_flag, so.emit_flag, "fe_ef_msum");
        stack.push_back(so.out_v);
        break;
      }
      case 37 /* STD_UPDATE */: {
        const rtbot::fuse::AuxArgs& a = pack.aux_args[arg];
        llvm::Value* x = pop(stack);
        StatefulOutput so = emit_stddev(ec, abs(a.a), a.b, dummy_t, x);
        emit_flag = b.CreateAnd(emit_flag, so.emit_flag, "fe_ef_std");
        stack.push_back(so.out_v);
        break;
      }
      case 38 /* DIFF */: {
        // FE state shape is 2 slots; emit_diff uses 4. Inline the FE-shaped
        // version here.
        llvm::Value* x   = pop(stack);
        FEDiffOut    res = emit_fe_diff(ec, abs(arg), x);
        emit_flag = b.CreateAnd(emit_flag, res.emit_flag, "fe_ef_diff");
        stack.push_back(res.out_v);
        break;
      }
      case 39 /* SIGN_CHANGE */: {
        llvm::Value* x = pop(stack);
        StatefulOutput so = emit_sign_change(ec, abs(arg), dummy_t, x);
        emit_flag = b.CreateAnd(emit_flag, so.emit_flag, "fe_ef_sc");
        stack.push_back(so.out_v);
        break;
      }
      case 40 /* WIN_MIN */: {
        const rtbot::fuse::AuxArgs& a = pack.aux_args[arg];
        llvm::Value* x = pop(stack);
        StatefulOutput so = emit_win_min(ec, abs(a.a), a.b, dummy_t, x);
        emit_flag = b.CreateAnd(emit_flag, so.emit_flag, "fe_ef_wmin");
        stack.push_back(so.out_v);
        break;
      }
      case 41 /* WIN_MAX */: {
        const rtbot::fuse::AuxArgs& a = pack.aux_args[arg];
        llvm::Value* x = pop(stack);
        StatefulOutput so = emit_win_max(ec, abs(a.a), a.b, dummy_t, x);
        emit_flag = b.CreateAnd(emit_flag, so.emit_flag, "fe_ef_wmax");
        stack.push_back(so.out_v);
        break;
      }
      case 42 /* FIR_UPDATE */: {
        const rtbot::fuse::AuxArgs& a = pack.aux_args[arg];
        const std::size_t W = a.b;
        const std::size_t coeff_off = a.c;
        if (coeff_off + W > coefficients.size()) {
          throw std::runtime_error(
              "emit_fused_expression: FIR coefficients out of range");
        }
        std::vector<double> slice(coefficients.begin() + coeff_off,
                                   coefficients.begin() + coeff_off + W);
        llvm::Value* x = pop(stack);
        StatefulOutput so = emit_fir(ec, abs(a.a), W, slice, dummy_t, x);
        emit_flag = b.CreateAnd(emit_flag, so.emit_flag, "fe_ef_fir");
        stack.push_back(so.out_v);
        break;
      }
      case 43 /* IIR_UPDATE */: {
        const rtbot::fuse::AuxArgs& a = pack.aux_args[arg];
        const std::size_t B = a.b;
        const std::size_t A = a.c;
        const std::size_t coeff_off = a.d;
        if (coeff_off + B + A > coefficients.size()) {
          throw std::runtime_error(
              "emit_fused_expression: IIR coefficients out of range");
        }
        std::vector<double> slice(coefficients.begin() + coeff_off,
                                   coefficients.begin() + coeff_off + B + A);
        llvm::Value* x = pop(stack);
        StatefulOutput so = emit_iir(ec, abs(a.a), B, A, slice, dummy_t, x);
        emit_flag = b.CreateAnd(emit_flag, so.emit_flag, "fe_ef_iir");
        stack.push_back(so.out_v);
        break;
      }

      case 44 /* GATE */: {
        // Predicate gate: AND emit_flag with (pred != 0). Stack reset to mirror
        // FE: each subsequent expression starts fresh.
        llvm::Value* pred  = pop(stack);
        llvm::Value* nonzero = b.CreateFCmpONE(
            pred, llvm::ConstantFP::get(f64, 0.0), "fe_gate_nz");
        emit_flag = b.CreateAnd(emit_flag, nonzero, "fe_ef_gate");
        stack.clear();
        break;
      }

      default:
        throw std::runtime_error(
            "emit_fused_expression: unknown opcode " + std::to_string(op));
    }
  }

  if (out_vs.size() != num_outputs) {
    throw std::runtime_error(
        "emit_fused_expression: bytecode produced " +
        std::to_string(out_vs.size()) +
        " outputs, expected " + std::to_string(num_outputs));
  }

  return FusedExprOutput{std::move(out_vs), emit_flag};
}

}  // namespace rtbot::jit::emit
