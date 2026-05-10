// test_burst_aggregate_parity.cpp
//
// Bit-exact parity tests for the JIT BurstAggregate operator.
//
// Each scenario builds the same program two ways:
//   (A) JIT pipeline:  Input(vector_number, width=N) -> BurstAggregate ->
//                       Output(width = K + M).
//   (B) FE BurstAggregate reference: instantiate the operator directly,
//                                     feed identical width-N VectorNumberData
//                                     rows, drain the collector.
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
#include "rtbot/fuse/BurstAggregate.h"
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

// Build JIT JSON for: Input(vector_number, width=N) -> BurstAggregate ->
// Output(width = K + M). Empty seg_bytecode is encoded as "segBytecode":[]
// (FE OperatorJson treats it as "no segment predicate"), but to keep this
// helper minimal we always supply both arrays.
std::string make_jit_json(std::size_t input_width,
                           const std::vector<double>& agg_bc,
                           const std::vector<double>& agg_consts,
                           const std::vector<double>& seg_bc,
                           const std::vector<double>& seg_consts,
                           const std::vector<std::size_t>& key_columns,
                           std::size_t num_agg_outputs) {
  const std::size_t out_w = key_columns.size() + num_agg_outputs;
  auto array_str = [](const std::vector<double>& v) {
    std::string s = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
      if (i > 0) s += ",";
      s += std::to_string(v[i]);
    }
    s += "]";
    return s;
  };
  auto idx_array_str = [](const std::vector<std::size_t>& v) {
    std::string s = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
      if (i > 0) s += ",";
      s += std::to_string(v[i]);
    }
    s += "]";
    return s;
  };

  std::string j;
  j  = R"({"title":"ba","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["vector_number"],"portWidths":[)" +
       std::to_string(input_width) + R"(]},)";
  j += R"({"id":"ba","type":"BurstAggregate")";
  j += R"(,"aggBytecode":)" + array_str(agg_bc);
  j += R"(,"aggConstants":)" + array_str(agg_consts);
  j += R"(,"segBytecode":)" + array_str(seg_bc);
  j += R"(,"segConstants":)" + array_str(seg_consts);
  j += R"(,"keyColumns":)" + idx_array_str(key_columns);
  j += R"(,"numAggOutputs":)" + std::to_string(num_agg_outputs);
  j += R"(,"numInputCols":)" + std::to_string(input_width);
  j += R"(},)";
  j += R"({"id":"out","type":"Output","portTypes":["vector_number"],"portWidths":[)" +
       std::to_string(out_w) + R"(]}],)";
  j += R"("connections":[)";
  j += R"({"from":"in","to":"ba","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"ba","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

// Run a sequence of width-N row inputs through the JIT. The emitted program
// function for vector-input programs (width > 1) has signature
// (state, t, const double* in_v_arr, ...); single-column programs use
// (state, t, double v, ...). The first row's width selects which signature
// to invoke; both bypass the scalar-only JitCompiledProgram::receive.
std::vector<EmitVec> run_jit(
    const std::string& json,
    const std::vector<std::pair<std::int64_t, std::vector<double>>>& stream) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  REQUIRE_FALSE(stream.empty());
  const bool vector_input = (stream.front().second.size() > 1);
  if (vector_input) {
    auto fn_vec_ptr = reinterpret_cast<std::int32_t (*)(
        double*, std::int64_t, double*, std::int64_t*, double*,
        std::int32_t*)>(prog->raw_fn());
    for (const auto& [t, v] : stream) {
      std::vector<double> in_buf = v;
      std::int32_t count =
          fn_vec_ptr(prog->raw_state(), t, in_buf.data(),
                     prog->raw_out_t_buf(), prog->raw_out_v_buf(),
                     prog->raw_out_port_id_buf());
      if (count > 0) {
        prog->push_emissions(count, prog->raw_out_t_buf(),
                              prog->raw_out_port_id_buf(),
                              prog->raw_out_v_buf(), prog->num_outputs());
      }
    }
  } else {
    auto fn_scalar_ptr = prog->raw_fn();
    for (const auto& [t, v] : stream) {
      std::int32_t count = fn_scalar_ptr(
          prog->raw_state(), t, v[0], prog->raw_out_t_buf(),
          prog->raw_out_v_buf(), prog->raw_out_port_id_buf());
      if (count > 0) {
        prog->push_emissions(count, prog->raw_out_t_buf(),
                              prog->raw_out_port_id_buf(),
                              prog->raw_out_v_buf(), prog->num_outputs());
      }
    }
  }
  std::vector<EmitVec> out;
  for (const auto& r : prog->collect_outputs()) {
    out.push_back({r.time, r.values});
  }
  return out;
}

// Run the FE BurstAggregate reference operator directly.
std::vector<EmitVec> run_fe(
    const std::vector<double>& agg_bc,
    const std::vector<double>& agg_consts,
    const std::vector<double>& seg_bc,
    const std::vector<double>& seg_consts,
    const std::vector<std::size_t>& key_columns,
    std::size_t num_agg_outputs,
    std::size_t num_input_cols,
    const std::vector<std::pair<std::int64_t, std::vector<double>>>& stream) {
  auto op = make_burst_aggregate(
      "ba", std::vector<double>{agg_bc}, std::vector<double>{agg_consts},
      std::vector<double>{seg_bc}, std::vector<double>{seg_consts},
      std::vector<std::size_t>{key_columns}, num_agg_outputs, num_input_cols);
  auto col = make_vector_number_collector("c");
  op->connect(col, 0, 0);

  std::vector<EmitVec> out;
  for (const auto& [t, v] : stream) {
    op->receive_data(create_message<VectorNumberData>(t, VectorNumberData(v)),
                      0);
    op->execute();
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

}  // namespace

// ---------------------------------------------------------------------------
// Scenario 1: Multiple transitions, AVG over single column.
//   width-1 vector. Aggregate: AVG(amplitude) via CUMSUM/COUNT/DIV.
//   Segment predicate: ABS(amplitude) > 0  (zero rows close the segment).
//   No key columns.
// ---------------------------------------------------------------------------
TEST_CASE("BurstAggregate JIT mean with zero-gate parity",
          "[jit][burst_aggregate][parity]") {
  std::vector<double> agg_bc = {INPUT, 0, CUMSUM, 0, COUNT, 2, DIV, END};
  std::vector<double> seg_bc = {INPUT, 0, ABS, CONST, 0, GT, END};
  std::vector<double> seg_consts = {0.0};

  // Stream: segments separated by zero-amplitude rows.
  //   t=1..3 amps {1,2,3}  → mean 2 at t=4
  //   t=4 amp 0            → mean 0 at t=5
  //   t=5..6 amps {10,20}  → mean 15 at t=7
  std::vector<std::pair<std::int64_t, std::vector<double>>> stream = {
      {1, {1.0}}, {2, {2.0}}, {3, {3.0}},
      {4, {0.0}}, {5, {10.0}}, {6, {20.0}},
      {7, {0.0}},
  };

  auto jit_out = run_jit(make_jit_json(1, agg_bc, {}, seg_bc, seg_consts,
                                        {}, 1),
                          stream);
  auto fe_out = run_fe(agg_bc, {}, seg_bc, seg_consts, {}, 1, 1, stream);

  REQUIRE(jit_out.size() == 3);
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 2: No segment transitions — empty seg_bytecode.
//   With no predicate, BurstAggregate accumulates indefinitely and never
//   emits. Both paths must produce zero records.
// ---------------------------------------------------------------------------
TEST_CASE("BurstAggregate JIT no-transition parity",
          "[jit][burst_aggregate][parity]") {
  std::vector<double> agg_bc = {INPUT, 0, CUMSUM, 0, COUNT, 2, DIV, END};
  std::vector<double> seg_bc = {};
  std::vector<double> seg_consts = {};

  std::vector<std::pair<std::int64_t, std::vector<double>>> stream = {
      {1, {1.0}}, {2, {2.0}}, {3, {3.0}}, {4, {4.0}},
  };

  auto jit_out = run_jit(make_jit_json(1, agg_bc, {}, seg_bc, seg_consts,
                                        {}, 1),
                          stream);
  auto fe_out = run_fe(agg_bc, {}, seg_bc, seg_consts, {}, 1, 1, stream);

  REQUIRE(jit_out.empty());
  REQUIRE(fe_out.empty());
}

// ---------------------------------------------------------------------------
// Scenario 3: Multiple key columns + multi-agg-output.
//   width-3 vector  [device_id, channel_id, amplitude].
//   Keys: [0, 1].
//   Aggregate outputs: COUNT, MAX_AGG, AVG.
//   Segment: ABS(amplitude) > 0.
// ---------------------------------------------------------------------------
TEST_CASE("BurstAggregate JIT keys + multi-agg parity",
          "[jit][burst_aggregate][parity]") {
  // agg_bytecode: COUNT 0; MAX_AGG INPUT 2 at slot 1; AVG = CUMSUM(input2)/COUNT.
  // State layout: [count(1), max(1), cumsum_kahan(2), count2(1)] — 5 slots.
  // (FE assigns state offsets per opcode in pack order.)
  std::vector<double> agg_bc = {
    COUNT, 0, END,
    INPUT, 2, MAX_AGG, 1, END,
    INPUT, 2, CUMSUM, 2, COUNT, 4, DIV, END,
  };
  std::vector<double> seg_bc = {INPUT, 2, ABS, CONST, 0, GT, END};
  std::vector<double> seg_consts = {0.0};

  std::vector<std::pair<std::int64_t, std::vector<double>>> stream = {
      {1, {7.0, 1.0, 2.0}},
      {2, {7.0, 1.0, 4.0}},
      {3, {7.0, 1.0, 6.0}},
      {4, {7.0, 1.0, 0.0}},   // close segment 1: count=3, max=6, avg=4
      {5, {7.0, 1.0, 1.0}},
      {6, {7.0, 1.0, 9.0}},
      {7, {7.0, 1.0, 0.0}},   // close segment 2 (well, segment of zeros)
  };

  auto jit_out = run_jit(make_jit_json(3, agg_bc, {}, seg_bc, seg_consts,
                                        {0, 1}, 3),
                          stream);
  auto fe_out = run_fe(agg_bc, {}, seg_bc, seg_consts, {0, 1}, 3, 3, stream);

  REQUIRE_FALSE(jit_out.empty());
  assert_match(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Scenario 4: Random fuzz — long stream with sporadic zero rows and a single
// running aggregate. Catches off-by-one / state-reset bugs that scenario-
// specific tests might miss.
// ---------------------------------------------------------------------------
TEST_CASE("BurstAggregate JIT fuzz parity",
          "[jit][burst_aggregate][parity][fuzz]") {
  std::vector<double> agg_bc = {INPUT, 0, CUMSUM, 0, COUNT, 2, DIV, END};
  std::vector<double> seg_bc = {INPUT, 0, ABS, CONST, 0, GT, END};
  std::vector<double> seg_consts = {0.0};

  std::mt19937_64 rng(0xC444u);
  std::uniform_real_distribution<double> dist(-50.0, 50.0);
  std::uniform_int_distribution<int> gate_dist(0, 4);
  std::vector<std::pair<std::int64_t, std::vector<double>>> stream;
  stream.reserve(200);
  for (std::size_t i = 1; i <= 200; ++i) {
    double v = (gate_dist(rng) == 0) ? 0.0 : dist(rng);
    stream.push_back({static_cast<std::int64_t>(i), {v}});
  }

  auto jit_out = run_jit(make_jit_json(1, agg_bc, {}, seg_bc, seg_consts,
                                        {}, 1),
                          stream);
  auto fe_out = run_fe(agg_bc, {}, seg_bc, seg_consts, {}, 1, 1, stream);

  assert_match(jit_out, fe_out);
}
