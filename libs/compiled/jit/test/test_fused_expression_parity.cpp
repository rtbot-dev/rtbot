// test_fused_expression_parity.cpp
//
// Bit-exact parity tests for the JIT FusedExpression operator.
//
// Each scenario builds the same FE program two ways:
//   (A) JIT pipeline:  Input(s) -> FusedExpression -> Output(width=numOutputs)
//                      driven through JitCompiler.
//   (B) FE C++ reference: instantiate FusedExpression directly, feed the same
//                          inputs, collect the emitted VectorNumberData values.
//
// We assert that the (timestamp, output values) of every emission match
// bit-exact between (A) and (B).

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/compiled/jit/JitCompiler.h"
#include "rtbot/compiled/jit/JitCompiledProgram.h"
#include "rtbot/fuse/FusedExpression.h"
#include "rtbot/fuse/FusedOps.h"

using namespace rtbot;
using namespace rtbot::fused_op;

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

struct Sample {
  std::int64_t t;
  std::vector<double> port_values;  // size == numPorts
};

struct EmitVec {
  std::int64_t t;
  std::vector<double> values;
};

// Build JIT JSON for: Input(N ports) -> FE -> Output(width=numOutputs).
// JIT JSON only supports a single Input op with portTypes; for multi-port
// FE inputs we instead use a dedicated Input per port and Identity passthroughs
// — but the JIT requires exactly one Input op. So we drive one Input op and
// fan it out through one Identity per port. To send distinct per-port data we
// instead use a Linear-style trick: each port j is fed via an AddScalar(0)
// branch from the single Input. To make the per-port values independent we
// can't use a single scalar input.
//
// The cleanest workaround: drive each FE port via its own data stream pushed
// through the JIT's Input operator on different ticks. But sync requires same
// timestamp on each port. So we use a multi-input Join-style program:
//   single Input -> identity -> FE port i (one connection per i)
//
// Then per-tick the SAME scalar value lands on all FE ports — useful for ops
// that compute scalar functions of `v`. For multi-port differentiated tests
// we precompute a hand-rolled bytecode of constants + INPUT 0.
//
// The simpler approach used here: drive a single-port stream and feed it to
// every FE port. Tests vary the bytecode (using CONST values or per-input
// indexing) to differentiate inputs.

// Build the FE-as-JIT-program JSON. Single Input -> N parallel Identity ops
// -> FE(numPorts=N) -> Output(width=numOutputs).
std::string make_jit_json(std::size_t num_ports, std::size_t num_outputs,
                           const std::vector<double>& bytecode,
                           const std::vector<double>& constants,
                           const std::vector<double>& coefficients = {}) {
  std::string j;
  j  = R"({"title":"fe","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  for (std::size_t i = 0; i < num_ports; ++i) {
    j += R"({"id":"id)" + std::to_string(i) + R"(","type":"Identity"},)";
  }
  j += R"({"id":"fe","type":"FusedExpression","numPorts":)" +
       std::to_string(num_ports) + R"(,"numOutputs":)" +
       std::to_string(num_outputs) + R"(,"bytecode":[)";
  for (std::size_t k = 0; k < bytecode.size(); ++k) {
    if (k > 0) j += ",";
    j += std::to_string(bytecode[k]);
  }
  j += R"(],"constants":[)";
  for (std::size_t k = 0; k < constants.size(); ++k) {
    if (k > 0) j += ",";
    j += std::to_string(constants[k]);
  }
  j += "]";
  if (!coefficients.empty()) {
    j += R"(,"coefficients":[)";
    for (std::size_t k = 0; k < coefficients.size(); ++k) {
      if (k > 0) j += ",";
      j += std::to_string(coefficients[k]);
    }
    j += "]";
  }
  j += R"(},)";
  j += R"({"id":"out","type":"Output","portTypes":["number"],"portWidths":[)" +
       std::to_string(num_outputs) + R"(]}],)";
  j += R"("connections":[)";
  for (std::size_t i = 0; i < num_ports; ++i) {
    j += R"({"from":"in","to":"id)" + std::to_string(i) +
         R"(","fromPort":"o1","toPort":"i1"},)";
    j += R"({"from":"id)" + std::to_string(i) +
         R"(","to":"fe","fromPort":"o1","toPort":"i)" +
         std::to_string(i + 1) + R"("},)";
  }
  j += R"({"from":"fe","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

// Run a sequence of single-scalar inputs through the JIT, collecting outputs.
std::vector<EmitVec> run_jit(const std::string& json,
                              const std::vector<std::pair<std::int64_t, double>>& stream) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  for (const auto& s : stream) prog->receive(s.first, s.second);
  std::vector<EmitVec> out;
  for (const auto& r : prog->collect_outputs()) {
    out.push_back({r.time, r.values});
  }
  return out;
}

// Run the FE C++ operator directly. Inputs are replicated across all FE ports
// (matching the JIT topology where one Input is fanned out to all ports).
std::vector<EmitVec> run_fe(std::size_t num_ports, std::size_t num_outputs,
                             std::vector<double> bytecode,
                             std::vector<double> constants,
                             std::vector<double> coefficients,
                             const std::vector<std::pair<std::int64_t, double>>& stream) {
  auto fe = make_fused_expression(
      "fe", num_ports, num_outputs, std::move(bytecode), std::move(constants),
      std::move(coefficients));
  auto col = make_vector_number_collector("c");
  fe->connect(col, 0, 0);

  std::vector<EmitVec> out;
  for (const auto& s : stream) {
    for (std::size_t p = 0; p < num_ports; ++p) {
      fe->receive_data(create_message<NumberData>(s.first, NumberData{s.second}), p);
    }
    fe->execute();
    auto& q = col->get_data_queue(0);
    while (!q.empty()) {
      auto* msg = static_cast<const Message<VectorNumberData>*>(q.front().get());
      EmitVec rec;
      rec.t = msg->time;
      rec.values = *msg->data.values;
      out.push_back(rec);
      q.pop_front();
    }
  }
  return out;
}

void assert_match(const std::vector<EmitVec>& a, const std::vector<EmitVec>& b) {
  REQUIRE(a.size() == b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    INFO("record " << i << " a.t=" << a[i].t << " b.t=" << b[i].t);
    REQUIRE(a[i].t == b[i].t);
    REQUIRE(a[i].values.size() == b[i].values.size());
    for (std::size_t k = 0; k < a[i].values.size(); ++k) {
      INFO("slot " << k << " a=" << a[i].values[k]
                   << " b=" << b[i].values[k]);
      REQUIRE(dbits(a[i].values[k]) == dbits(b[i].values[k]));
    }
  }
}

std::vector<std::pair<std::int64_t, double>> make_stream(std::uint64_t seed,
                                                          std::size_t n = 60) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  std::vector<std::pair<std::int64_t, double>> out;
  out.reserve(n);
  for (std::size_t i = 1; i <= n; ++i) {
    out.push_back({static_cast<std::int64_t>(i), dist(rng)});
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Scenario 1: basic arithmetic — INPUT 0, INPUT 1, MUL, END.
// Single Input fanned out to both ports, so result is v * v.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT basic arithmetic parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 2;
  const std::size_t M = 1;
  std::vector<double> bc = {INPUT, 0, INPUT, 1, MUL, END};
  std::vector<double> consts = {};

  auto stream = make_stream(0x1111u);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  REQUIRE(jit_out.front().values.size() == M);
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 2: multi-output passthrough + multiply.
//   numPorts=3, numOutputs=2.
//   bytecode: INPUT 0, END,    INPUT 1, INPUT 2, MUL, END.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT multi-output parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 3;
  const std::size_t M = 2;
  std::vector<double> bc = {
    INPUT, 0, END,
    INPUT, 1, INPUT, 2, MUL, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0x2222u);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  REQUIRE(jit_out.front().values.size() == M);
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 3: constants — (input + 5) * 2.
//   bytecode: INPUT 0, CONST 0, ADD, CONST 1, MUL, END.
//   constants: [5.0, 2.0].
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT constants parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 1;
  std::vector<double> bc = {
    INPUT, 0, CONST, 0, ADD, CONST, 1, MUL, END,
  };
  std::vector<double> consts = {5.0, 2.0};

  auto stream = make_stream(0x3333u);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 4: stateful CUMSUM (Kahan-compensated). 2 outputs:
//   col0 = CUMSUM(input)
//   col1 = COUNT()
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT CUMSUM+COUNT parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 2;
  // CUMSUM uses 2 state slots starting at offset 0; COUNT uses 1 slot at offset 2.
  std::vector<double> bc = {
    INPUT, 0, CUMSUM, 0, END,
    COUNT, 2, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0x4444u);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  REQUIRE(jit_out.size() == stream.size());
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 5: stateful MA_UPDATE — moving average over window 5. The FE
// suppresses output while count < 5; JIT must do the same.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT MA_UPDATE warmup parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 1;
  std::vector<double> bc = {
    INPUT, 0, MA_UPDATE, 5, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0x5555u, 30);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  // First W-1 ticks are suppressed; remainder match bit-exactly.
  REQUIRE(jit_out.size() == stream.size() - 4);
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 6: parallel windowed ops — MA(5) and STD(5) in the same FE.
// Two END markers; both warm up at the same time.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT MA+STD parallel parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 2;
  std::vector<double> bc = {
    INPUT, 0, MA_UPDATE,  5, END,
    INPUT, 0, STD_UPDATE, 5, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0x6666u, 40);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 7: GATE — predicate suppressing emission when input <= 0.
//   bytecode: INPUT 0, CONST 0, GT, GATE,    INPUT 0, END.
//   constants: [0.0].
// FE GATE pops the predicate, suppresses emission for the tick if zero,
// then resets the stack; subsequent expression is INPUT 0 -> END.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT GATE parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 1;
  std::vector<double> bc = {
    INPUT, 0, CONST, 0, GT, GATE,
    INPUT, 0, END,
  };
  std::vector<double> consts = {0.0};

  auto stream = make_stream(0x7777u, 50);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  // Some emissions are suppressed by GATE; sizes must still match.
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 8: FIR_UPDATE with realistic 3-tap symmetric averaging filter.
// Coefficients are stored in the FE's coefficient table; FIR_UPDATE inline
// args (coeff_start=0, coeff_len=3) reference them.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT FIR_UPDATE parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 1;
  std::vector<double> bc = {
    INPUT, 0, FIR_UPDATE, 0, 3, END,
  };
  std::vector<double> consts = {};
  std::vector<double> coefs  = {0.25, 0.5, 0.25};

  auto stream = make_stream(0x8888u, 40);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts, coefs), stream);
  auto fe_out  = run_fe (N, M, bc, consts, coefs, stream);

  REQUIRE_FALSE(jit_out.empty());
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 9: IIR_UPDATE — 1st-order IIR low-pass.
//   y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]
// IIR_UPDATE inline args: (b_len=2, a_len=1, coeff_start=0). Coefficients are
// laid out as [b0, b1, a1].
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT IIR_UPDATE parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 1;
  std::vector<double> bc = {
    INPUT, 0, IIR_UPDATE, 2, 1, 0, END,
  };
  std::vector<double> consts = {};
  std::vector<double> coefs  = {0.5, 0.5, 0.2};

  auto stream = make_stream(0x9999u, 40);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts, coefs), stream);
  auto fe_out  = run_fe (N, M, bc, consts, coefs, stream);

  REQUIRE_FALSE(jit_out.empty());
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 10: WIN_MIN / WIN_MAX windowed extremes.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT WIN_MIN/WIN_MAX parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 2;
  std::vector<double> bc = {
    INPUT, 0, WIN_MIN, 4, END,
    INPUT, 0, WIN_MAX, 4, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0xAAAAu, 30);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 11: DIFF + SIGN_CHANGE — both 2-state-slot opcodes that suppress
// the first-sample emission. Two END markers.
// State layout: DIFF at offset 0 (slots 0,1), SIGN_CHANGE at offset 2 (slots 2,3).
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT DIFF+SIGN_CHANGE parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 2;
  std::vector<double> bc = {
    INPUT, 0, DIFF, 0, END,
    INPUT, 0, SIGN_CHANGE, 2, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0xBBBBu, 30);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  // First sample is suppressed.
  REQUIRE(jit_out.size() == stream.size() - 1);
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 12: comparison and boolean opcodes.
//   col0 = (input > 0) AND (input < 50) ? 1.0 : 0.0
//   col1 = NOT(input == 0)
// constants: [0.0, 50.0]
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT comparison/boolean parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 2;
  std::vector<double> bc = {
    INPUT, 0, CONST, 0, GT, INPUT, 0, CONST, 1, LT, AND, END,
    INPUT, 0, CONST, 0, EQ, NOT, END,
  };
  std::vector<double> consts = {0.0, 50.0};

  auto stream = make_stream(0xCCCCu, 50);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 13: aggregates MAX_AGG / MIN_AGG with -inf / +inf seeds.
//   col0 = MAX_AGG over input
//   col1 = MIN_AGG over input
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT MAX_AGG/MIN_AGG parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 2;
  std::vector<double> bc = {
    INPUT, 0, MAX_AGG, 0, END,
    INPUT, 0, MIN_AGG, 1, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0xDDDDu, 40);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  REQUIRE(jit_out.size() == stream.size());
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 14: STATE_LOAD reads a slot owned by a CUMSUM in the same program.
//   col0 = CUMSUM(input)            (writes state[0])
//   col1 = STATE_LOAD slot 0        (reads same slot)
// Both columns must be identical.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpression JIT STATE_LOAD parity",
          "[jit][fused_expression][parity]") {
  const std::size_t N = 1;
  const std::size_t M = 2;
  std::vector<double> bc = {
    INPUT, 0, CUMSUM, 0, END,
    STATE_LOAD, 0, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0xEEEEu, 30);

  auto jit_out = run_jit(make_jit_json(N, M, bc, consts), stream);
  auto fe_out  = run_fe (N, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  REQUIRE(jit_out.size() == stream.size());
  for (const auto& rec : jit_out) {
    REQUIRE(dbits(rec.values[0]) == dbits(rec.values[1]));
  }
  assert_match(jit_out, fe_out);
}
