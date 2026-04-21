#include "fused_parity/fuzz_bytecode.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

#include "rtbot/fuse/FusedExpression.h"

namespace rtbot::fused_parity {

using namespace rtbot::fused_op;

namespace {

// Build an RPN subexpression that leaves exactly one value on the stack.
// max_depth bounds recursion; at depth==0 the tree must be a leaf (INPUT or
// CONST). Each opcode appended also sanitizes operand domains so the live
// and reference evaluators cannot diverge on NaN semantics.
//
// emit_transcendentals / emit_stateful toggle inclusion of LOG/EXP/etc and
// CUMSUM/COUNT/... The stateful case also consumes state slots via the
// state_cursor output parameter.
void emit_expression(std::mt19937_64& rng,
                     std::size_t num_inputs,
                     std::size_t num_constants,
                     int max_depth,
                     bool emit_transcendentals,
                     bool emit_stateful,
                     std::vector<double>& bc,
                     std::size_t& state_cursor) {
  std::uniform_int_distribution<int> leaf_pick(0, 1);
  if (max_depth <= 0) {
    if (leaf_pick(rng) == 0 || num_constants == 0) {
      std::uniform_int_distribution<std::size_t> input_idx(0, num_inputs - 1);
      bc.push_back(INPUT);
      bc.push_back(static_cast<double>(input_idx(rng)));
    } else {
      std::uniform_int_distribution<std::size_t> const_idx(0, num_constants - 1);
      bc.push_back(CONST);
      bc.push_back(static_cast<double>(const_idx(rng)));
    }
    return;
  }

  // Assemble the list of operator kinds available at this depth.
  enum Kind {
    // binary arithmetic
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV_CONST, BIN_POW_ABS,
    // unary
    UN_NEG, UN_ABS, UN_FLOOR, UN_CEIL, UN_ROUND, UN_NOT,
    UN_SQRT_ABS, UN_LOG_ABSP, UN_LOG10_ABSP, UN_EXP_CLAMP,
    UN_SIN, UN_COS, UN_TAN_CLAMP,
    // comparison
    CMP_GT, CMP_GTE, CMP_LT, CMP_LTE, CMP_EQ, CMP_NEQ,
    // boolean
    BOOL_AND, BOOL_OR,
    // stateful
    ST_CUMSUM, ST_COUNT, ST_MAX, ST_MIN,
    // leaf fallback
    LEAF,
  };
  std::vector<Kind> pool = {
      BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV_CONST,
      UN_NEG, UN_ABS, UN_FLOOR, UN_CEIL, UN_ROUND, UN_NOT,
      CMP_GT, CMP_GTE, CMP_LT, CMP_LTE, CMP_EQ, CMP_NEQ,
      BOOL_AND, BOOL_OR,
      LEAF, LEAF,  // higher leaf weight to prevent runaway depth
  };
  if (emit_transcendentals) {
    pool.insert(pool.end(), {BIN_POW_ABS, UN_SQRT_ABS, UN_LOG_ABSP,
                             UN_LOG10_ABSP, UN_EXP_CLAMP, UN_SIN, UN_COS,
                             UN_TAN_CLAMP});
  }
  if (emit_stateful) {
    pool.insert(pool.end(), {ST_CUMSUM, ST_COUNT, ST_MAX, ST_MIN});
  }

  std::uniform_int_distribution<std::size_t> pick(0, pool.size() - 1);
  Kind k = pool[pick(rng)];

  auto emit_sub = [&](int depth) {
    emit_expression(rng, num_inputs, num_constants, depth, emit_transcendentals,
                    emit_stateful, bc, state_cursor);
  };

  switch (k) {
    case LEAF: {
      emit_sub(0);
      return;
    }
    case BIN_ADD:
    case BIN_SUB:
    case BIN_MUL: {
      emit_sub(max_depth - 1);
      emit_sub(max_depth - 1);
      bc.push_back(k == BIN_ADD ? ADD : k == BIN_SUB ? SUB : MUL);
      return;
    }
    case BIN_DIV_CONST: {
      emit_sub(max_depth - 1);
      // Divide by a fresh constant known non-zero. Caller ensures constants
      // vector has at least one non-zero entry, but we defensively append
      // a non-zero CONST inline via CONST <idx>. We require idx 0 is reserved
      // for "1.0" in the caller; use it.
      bc.push_back(CONST);
      bc.push_back(0.0);
      bc.push_back(DIV);
      return;
    }
    case BIN_POW_ABS: {
      // (|a|) ^ (b mod 4 rounded). Keeps base non-negative and exponent
      // bounded so pow() stays well-defined and finite.
      emit_sub(max_depth - 1);
      bc.push_back(ABS);
      emit_sub(max_depth - 1);
      bc.push_back(CONST);
      bc.push_back(1.0);  // constants index 1 reserved for 4.0 below
      // modulo via arithmetic: x - floor(x/4)*4
      // Simpler: use a small integer exponent literal via a second CONST. We
      // keep a fixed exponent set (0.5, 1.0, 2.0, 3.0) to avoid numerical
      // surprises, indexed 2..5.
      bc.pop_back(); bc.pop_back();  // drop the CONST 1.0 just queued
      std::uniform_int_distribution<int> exp_pick(2, 5);
      bc.push_back(CONST);
      bc.push_back(static_cast<double>(exp_pick(rng)));
      bc.push_back(POW);
      return;
    }
    case UN_NEG:
    case UN_ABS:
    case UN_FLOOR:
    case UN_CEIL:
    case UN_ROUND:
    case UN_NOT: {
      emit_sub(max_depth - 1);
      bc.push_back(k == UN_NEG   ? NEG
                   : k == UN_ABS ? ABS
                   : k == UN_FLOOR ? FLOOR
                   : k == UN_CEIL  ? CEIL
                   : k == UN_ROUND ? ROUND
                                   : NOT);
      return;
    }
    case UN_SQRT_ABS: {
      emit_sub(max_depth - 1);
      bc.push_back(ABS);
      bc.push_back(SQRT);
      return;
    }
    case UN_LOG_ABSP: {
      // log(|x| + 1)
      emit_sub(max_depth - 1);
      bc.push_back(ABS);
      bc.push_back(CONST);
      bc.push_back(6.0);  // idx 6 = 1.0
      bc.push_back(ADD);
      bc.push_back(LOG);
      return;
    }
    case UN_LOG10_ABSP: {
      emit_sub(max_depth - 1);
      bc.push_back(ABS);
      bc.push_back(CONST);
      bc.push_back(6.0);
      bc.push_back(ADD);
      bc.push_back(LOG10);
      return;
    }
    case UN_EXP_CLAMP: {
      // exp(sin(x)) — sin in [-1,1], exp domain safe.
      emit_sub(max_depth - 1);
      bc.push_back(SIN);
      bc.push_back(EXP);
      return;
    }
    case UN_SIN:
    case UN_COS: {
      emit_sub(max_depth - 1);
      bc.push_back(k == UN_SIN ? SIN : COS);
      return;
    }
    case UN_TAN_CLAMP: {
      // tan(sin(x)) — sin keeps arg away from pi/2 singularity.
      emit_sub(max_depth - 1);
      bc.push_back(SIN);
      bc.push_back(TAN);
      return;
    }
    case CMP_GT:
    case CMP_GTE:
    case CMP_LT:
    case CMP_LTE:
    case CMP_EQ:
    case CMP_NEQ: {
      emit_sub(max_depth - 1);
      emit_sub(max_depth - 1);
      bc.push_back(k == CMP_GT  ? GT
                   : k == CMP_GTE ? GTE
                   : k == CMP_LT  ? LT
                   : k == CMP_LTE ? LTE
                   : k == CMP_EQ  ? EQ
                                  : NEQ);
      return;
    }
    case BOOL_AND:
    case BOOL_OR: {
      emit_sub(max_depth - 1);
      emit_sub(max_depth - 1);
      bc.push_back(k == BOOL_AND ? AND : OR);
      return;
    }
    case ST_CUMSUM: {
      emit_sub(max_depth - 1);
      bc.push_back(CUMSUM);
      bc.push_back(static_cast<double>(state_cursor));
      state_cursor += 2;
      return;
    }
    case ST_COUNT: {
      // COUNT does not pop; make sure the expression still leaves one value.
      // Emit a sub-expression, then DROP it via MUL with 0 and ADD COUNT — or
      // more simply, skip the sub and just emit COUNT at a depth-0 leaf slot.
      bc.push_back(COUNT);
      bc.push_back(static_cast<double>(state_cursor));
      state_cursor += 1;
      return;
    }
    case ST_MAX: {
      emit_sub(max_depth - 1);
      bc.push_back(MAX_AGG);
      bc.push_back(static_cast<double>(state_cursor));
      state_cursor += 1;
      return;
    }
    case ST_MIN: {
      emit_sub(max_depth - 1);
      bc.push_back(MIN_AGG);
      bc.push_back(static_cast<double>(state_cursor));
      state_cursor += 1;
      return;
    }
  }
}

}  // namespace

FuzzProgram generate_program(std::uint64_t seed,
                             std::size_t max_inputs,
                             std::size_t max_outputs,
                             bool include_transcendentals,
                             bool include_stateful) {
  std::mt19937_64 rng(seed);
  FuzzProgram p;

  std::uniform_int_distribution<std::size_t> n_in(1, std::max<std::size_t>(1, max_inputs));
  std::uniform_int_distribution<std::size_t> n_out(1, std::max<std::size_t>(1, max_outputs));
  p.num_inputs = n_in(rng);
  p.num_outputs = n_out(rng);

  // Reserved constants:
  //   0 → 1.0 (used as safe divisor)
  //   1..5 → small integer exponents 0.5, 1.0, 2.0, 3.0 (POW uses index 2..5,
  //          plus 1 as scratch)
  //   6 → 1.0 (used by LOG/LOG10 wrappers)
  //   7..7+N → random constants
  p.constants = {1.0, 1.0, 0.5, 1.0, 2.0, 3.0, 1.0};
  std::uniform_real_distribution<double> const_dist(-10.0, 10.0);
  std::uniform_int_distribution<int> extra_const_count(0, 4);
  int extra = extra_const_count(rng);
  for (int i = 0; i < extra; ++i) p.constants.push_back(const_dist(rng));

  // State for stateful opcodes.
  std::size_t state_cursor = 0;

  std::uniform_int_distribution<int> depth_dist(1, 6);
  for (std::size_t k = 0; k < p.num_outputs; ++k) {
    int depth = depth_dist(rng);
    emit_expression(rng, p.num_inputs, p.constants.size(), depth,
                    include_transcendentals, include_stateful, p.bytecode,
                    state_cursor);
    p.bytecode.push_back(END);
  }

  p.state_init.assign(state_cursor,
                      std::numeric_limits<double>::signaling_NaN());
  // Initialize state slots per opcode conventions.
  // We don't track which slot belongs to which opcode; default to 0 is safe
  // only for CUMSUM/COUNT. MAX/MIN require ±inf seeds.
  // To stay simple and correct, we re-walk bytecode and set slots.
  std::fill(p.state_init.begin(), p.state_init.end(), 0.0);
  std::size_t pc = 0;
  while (pc < p.bytecode.size()) {
    int op = static_cast<int>(p.bytecode[pc++]);
    bool has_arg = (op == (int)INPUT || op == (int)CONST ||
                    op == (int)CUMSUM || op == (int)COUNT ||
                    op == (int)MAX_AGG || op == (int)MIN_AGG ||
                    op == (int)STATE_LOAD);
    if (!has_arg) continue;
    std::size_t arg = static_cast<std::size_t>(p.bytecode[pc++]);
    if (op == (int)MAX_AGG) {
      p.state_init[arg] = -std::numeric_limits<double>::infinity();
    } else if (op == (int)MIN_AGG) {
      p.state_init[arg] = std::numeric_limits<double>::infinity();
    }
  }

  return p;
}

std::vector<std::vector<double>> generate_input_sequence(
    std::uint64_t seed, std::size_t num_inputs, std::size_t num_messages) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(-1e3, 1e3);
  std::vector<std::vector<double>> out(num_messages,
                                         std::vector<double>(num_inputs));
  for (auto& msg : out) {
    for (auto& v : msg) v = dist(rng);
  }
  return out;
}

}  // namespace rtbot::fused_parity
