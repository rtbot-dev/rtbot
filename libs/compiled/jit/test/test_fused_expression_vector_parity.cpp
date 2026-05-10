// test_fused_expression_vector_parity.cpp
//
// Bit-exact parity tests for the JIT FusedExpressionVector operator.
//
// Each scenario builds the same FE-Vector program two ways:
//   (A) JIT pipeline:  Input -> N scalar branches (Identity/AddScalar) ->
//                       VectorCompose(N) -> FusedExpressionVector ->
//                       Output(width = numOutputs).
//   (B) FE-Vector C++ reference: instantiate FusedExpressionVector directly,
//                                 feed identical width-N VectorNumberData
//                                 messages, collect emitted vectors.
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
#include "rtbot/fuse/FusedExpressionVector.h"
#include "rtbot/fuse/FusedOps.h"

using namespace rtbot;
using namespace rtbot::fused_op;

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

struct EmitVec {
  std::int64_t t;
  std::vector<double> values;
};

// Build JIT JSON for: Input -> N scalar branches -> VectorCompose(N) ->
// FusedExpressionVector -> Output(width = numOutputs).
//
// Each branch i is `Add(value=shifts[i])` (or Identity when shifts[i] == 0)
// so the assembled vector lane k carries (input + shifts[k]).
std::string make_jit_json(const std::vector<double>& shifts,
                           std::size_t num_outputs,
                           const std::vector<double>& bytecode,
                           const std::vector<double>& constants,
                           const std::vector<double>& coefficients = {}) {
  const std::size_t N = shifts.size();
  std::string j;
  j  = R"({"title":"fev","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  for (std::size_t i = 0; i < N; ++i) {
    if (shifts[i] == 0.0) {
      j += R"({"id":"b)" + std::to_string(i) + R"(","type":"Identity"},)";
    } else {
      j += R"({"id":"b)" + std::to_string(i) +
           R"(","type":"Add","value":)" + std::to_string(shifts[i]) + "},";
    }
  }
  j += R"({"id":"vc","type":"VectorCompose","numPorts":)" +
       std::to_string(N) + R"(},)";
  j += R"({"id":"fev","type":"FusedExpressionVector","numOutputs":)" +
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
  for (std::size_t i = 0; i < N; ++i) {
    j += R"({"from":"in","to":"b)" + std::to_string(i) +
         R"(","fromPort":"o1","toPort":"i1"},)";
    j += R"({"from":"b)" + std::to_string(i) +
         R"(","to":"vc","fromPort":"o1","toPort":"i)" +
         std::to_string(i + 1) + R"("},)";
  }
  j += R"({"from":"vc","to":"fev","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"fev","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

// Run a sequence of scalar inputs through the JIT, collecting outputs.
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

// Run the FE-Vector C++ operator directly with a per-tick width-N vector
// input matching the JIT topology (lane k = input + shifts[k]).
std::vector<EmitVec> run_fev(const std::vector<double>& shifts,
                              std::size_t num_outputs,
                              std::vector<double> bytecode,
                              std::vector<double> constants,
                              std::vector<double> coefficients,
                              const std::vector<std::pair<std::int64_t, double>>& stream) {
  const std::size_t N = shifts.size();
  auto fev = make_fused_expression_vector(
      "fev", num_outputs, std::move(bytecode), std::move(constants),
      std::move(coefficients));
  auto col = make_vector_number_collector("c");
  fev->connect(col, 0, 0);

  std::vector<EmitVec> out;
  for (const auto& s : stream) {
    std::vector<double> lanes(N);
    for (std::size_t k = 0; k < N; ++k) lanes[k] = s.second + shifts[k];
    fev->receive_data(
        create_message<VectorNumberData>(s.first, VectorNumberData(lanes)), 0);
    fev->execute();
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
// Scenario 1: basic arithmetic — width-3 vector, multiply lanes 0 and 1.
//   bytecode: INPUT 0, INPUT 1, MUL, END.
//   Result lane: (input + shifts[0]) * (input + shifts[1]).
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpressionVector JIT basic arithmetic parity",
          "[jit][fused_expression_vector][parity]") {
  std::vector<double> shifts{0.0, 1.5, -2.25};
  const std::size_t M = 1;
  std::vector<double> bc = {INPUT, 0, INPUT, 1, MUL, END};
  std::vector<double> consts = {};

  auto stream = make_stream(0xA111u);

  auto jit_out = run_jit(make_jit_json(shifts, M, bc, consts), stream);
  auto fev_out = run_fev(shifts, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  REQUIRE(jit_out.front().values.size() == M);
  assert_match(jit_out, fev_out);
}

// ---------------------------------------------------------------------------
// Scenario 2: multi-output passthrough + multiply.
//   width-3 vector, two ENDs:
//     col0 = INPUT 0 (passthrough lane 0)
//     col1 = INPUT 1 * INPUT 2
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpressionVector JIT multi-output parity",
          "[jit][fused_expression_vector][parity]") {
  std::vector<double> shifts{0.0, -3.0, 7.0};
  const std::size_t M = 2;
  std::vector<double> bc = {
    INPUT, 0, END,
    INPUT, 1, INPUT, 2, MUL, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0xA222u);

  auto jit_out = run_jit(make_jit_json(shifts, M, bc, consts), stream);
  auto fev_out = run_fev(shifts, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  REQUIRE(jit_out.front().values.size() == M);
  assert_match(jit_out, fev_out);
}

// ---------------------------------------------------------------------------
// Scenario 3: stateful CUMSUM on lane 0, COUNT alongside.
//   width-2 vector. CUMSUM uses 2 state slots (offset 0); COUNT uses 1
//   slot at offset 2.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpressionVector JIT CUMSUM+COUNT parity",
          "[jit][fused_expression_vector][parity]") {
  std::vector<double> shifts{0.0, 4.5};
  const std::size_t M = 2;
  std::vector<double> bc = {
    INPUT, 0, CUMSUM, 0, END,
    COUNT, 2, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0xA333u, 30);

  auto jit_out = run_jit(make_jit_json(shifts, M, bc, consts), stream);
  auto fev_out = run_fev(shifts, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  REQUIRE(jit_out.size() == stream.size());
  assert_match(jit_out, fev_out);
}

// ---------------------------------------------------------------------------
// Scenario 4: stateful MA_UPDATE on lane 1, with warmup-suppression.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpressionVector JIT MA_UPDATE warmup parity",
          "[jit][fused_expression_vector][parity]") {
  std::vector<double> shifts{0.0, 2.0};
  const std::size_t M = 1;
  std::vector<double> bc = {
    INPUT, 1, MA_UPDATE, 5, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0xA444u, 30);

  auto jit_out = run_jit(make_jit_json(shifts, M, bc, consts), stream);
  auto fev_out = run_fev(shifts, M, bc, consts, {}, stream);

  // First W-1 ticks are suppressed; the remainder must match bit-exactly.
  REQUIRE(jit_out.size() == stream.size() - 4);
  assert_match(jit_out, fev_out);
}

// ---------------------------------------------------------------------------
// Scenario 5: comparison + GATE — emit only when lane 0 > 0 AND lane 1 < 50.
//   bytecode: INPUT 0, CONST 0, GT, INPUT 1, CONST 1, LT, AND, GATE,
//             INPUT 0, INPUT 1, ADD, END.
//   constants: [0.0, 50.0].
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpressionVector JIT GATE parity",
          "[jit][fused_expression_vector][parity]") {
  std::vector<double> shifts{0.0, 10.0};
  const std::size_t M = 1;
  std::vector<double> bc = {
    INPUT, 0, CONST, 0, GT, INPUT, 1, CONST, 1, LT, AND, GATE,
    INPUT, 0, INPUT, 1, ADD, END,
  };
  std::vector<double> consts = {0.0, 50.0};

  auto stream = make_stream(0xA555u, 50);

  auto jit_out = run_jit(make_jit_json(shifts, M, bc, consts), stream);
  auto fev_out = run_fev(shifts, M, bc, consts, {}, stream);

  // GATE may suppress some emissions; sizes must still match.
  assert_match(jit_out, fev_out);
}

// ---------------------------------------------------------------------------
// Scenario 6: constants and transcendentals on a width-2 vector.
//   col0 = SQRT(INPUT 0 * INPUT 0 + INPUT 1 * INPUT 1)   (Euclidean norm)
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpressionVector JIT norm parity",
          "[jit][fused_expression_vector][parity]") {
  std::vector<double> shifts{0.0, -5.5};
  const std::size_t M = 1;
  std::vector<double> bc = {
    INPUT, 0, INPUT, 0, MUL,
    INPUT, 1, INPUT, 1, MUL,
    ADD, SQRT, END,
  };
  std::vector<double> consts = {};

  auto stream = make_stream(0xA666u);

  auto jit_out = run_jit(make_jit_json(shifts, M, bc, consts), stream);
  auto fev_out = run_fev(shifts, M, bc, consts, {}, stream);

  REQUIRE_FALSE(jit_out.empty());
  assert_match(jit_out, fev_out);
}

namespace {

// Build a JIT JSON that mirrors the rtbot-sql preset shape:
//   Input(vector_number, width=N)  ->  FusedExpressionVector  ->  Output(width=M)
// No VectorCompose intermediary — the program-level Input op feeds the FEV
// directly with a width-N vector wire.
std::string make_linear_jit_json(std::size_t input_width,
                                  std::size_t num_outputs,
                                  const std::vector<double>& bytecode,
                                  const std::vector<double>& constants) {
  std::string j;
  j  = R"({"title":"fev-linear","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["vector_number"],"portWidths":[)" +
       std::to_string(input_width) + R"(]},)";
  j += R"({"id":"fev","type":"FusedExpressionVector","numOutputs":)" +
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
  j += R"(]},)";
  j += R"({"id":"out","type":"Output","portTypes":["vector_number"],"portWidths":[)" +
       std::to_string(num_outputs) + R"(]}],)";
  j += R"("connections":[)";
  j += R"({"from":"in","to":"fev","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"fev","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

// Run a sequence of vector inputs (lane buffers) through the JIT program.
// The emitted program function for vector-input programs has signature
// (state, t, const double* in_v_arr, ...) so we cast raw_fn() and pass
// the lane buffer directly.
std::vector<EmitVec> run_jit_vector_input(
    const std::string& json,
    const std::vector<std::pair<std::int64_t, std::vector<double>>>& stream) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  auto fn_vec_ptr = reinterpret_cast<std::int32_t (*)(
      double*, std::int64_t, double*, std::int64_t*, double*, std::int32_t*)>(
      prog->raw_fn());
  for (const auto& s : stream) {
    std::vector<double> in_buf = s.second;
    std::int32_t count =
        fn_vec_ptr(prog->raw_state(), s.first, in_buf.data(),
                   prog->raw_out_t_buf(), prog->raw_out_v_buf(),
                   prog->raw_out_port_id_buf());
    if (count > 0) {
      prog->push_emissions(count, prog->raw_out_t_buf(),
                            prog->raw_out_port_id_buf(),
                            prog->raw_out_v_buf(), prog->num_outputs());
    }
  }
  std::vector<EmitVec> out;
  for (const auto& r : prog->collect_outputs()) {
    out.push_back({r.time, r.values});
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Scenario 7: Linear vector-input program — exactly the rtbot-sql preset
// shape. `Input(vector_number, width=N) -> FusedExpressionVector -> Output`
// with no VectorCompose intermediary. This ensures the JIT routes such
// graphs through the vector-input emission path even though the partition
// has no sync ops.
// ---------------------------------------------------------------------------
TEST_CASE("FusedExpressionVector JIT linear vector-input parity",
          "[jit][fused_expression_vector][parity][linear]") {
  const std::size_t N = 3;
  const std::size_t M = 2;
  // col0 = lane0 + lane2,   col1 = lane1 * lane2
  std::vector<double> bc = {
    INPUT, 0, INPUT, 2, ADD, END,
    INPUT, 1, INPUT, 2, MUL, END,
  };
  std::vector<double> consts = {};

  std::mt19937_64 rng(0xB777u);
  std::uniform_real_distribution<double> dist(-50.0, 50.0);
  std::vector<std::pair<std::int64_t, std::vector<double>>> stream;
  for (std::size_t i = 1; i <= 40; ++i) {
    std::vector<double> lanes(N);
    for (std::size_t k = 0; k < N; ++k) lanes[k] = dist(rng);
    stream.push_back({static_cast<std::int64_t>(i), std::move(lanes)});
  }

  auto jit_out = run_jit_vector_input(
      make_linear_jit_json(N, M, bc, consts), stream);

  // Reference: run the FE BurstAggregate-less FEV operator directly.
  auto fev_ref = make_fused_expression_vector("fev", M, bc, consts, {});
  auto col_ref = make_vector_number_collector("c");
  fev_ref->connect(col_ref, 0, 0);
  std::vector<EmitVec> fev_out;
  for (const auto& s : stream) {
    fev_ref->receive_data(
        create_message<VectorNumberData>(s.first, VectorNumberData(s.second)),
        0);
    fev_ref->execute();
    auto& q = col_ref->get_data_queue(0);
    while (!q.empty()) {
      auto* msg = static_cast<const Message<VectorNumberData>*>(q.front().get());
      EmitVec rec;
      rec.t = msg->time;
      rec.values = *msg->data.values;
      fev_out.push_back(rec);
      q.pop_front();
    }
  }

  REQUIRE_FALSE(jit_out.empty());
  REQUIRE(jit_out.front().values.size() == M);
  assert_match(jit_out, fev_out);
}
