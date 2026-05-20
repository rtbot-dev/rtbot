// test_topk_parity.cpp
//
// Bit-exact FE-vs-JIT parity tests for the TopK operator.
//
// The JIT is driven by a `Input -> branches -> VectorCompose -> TopK -> Output`
// pipeline so the per-tick scalar `prog->receive(t, v)` API can synthesize a
// width-N vector row before TopK consumes it.
//
// The reference is an FE TopK driven directly with the same Message<VectorNumberData>
// rows. Both produce 1 emit per row in warmup (count grows 1..K) then K emits
// per tick at steady state.

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Message.h"
#include "rtbot/std/TopK.h"
#include "rtbot/compiled/jit/JitCompiler.h"
#include "rtbot/compiled/jit/JitCompiledProgram.h"

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

struct Sample { std::int64_t t; double v; };

struct EmitRow {
  std::int64_t t;
  std::vector<double> values;
};

// ----- JIT pipeline JSON builders -------------------------------------------

// Each branch transforms the input scalar v into a distinct lane value. Lane 0
// uses Identity (lane = v); lane 1 uses Add(shift) (lane = v + shift); etc.
// shifts.size() == row width.
std::string branches_json(const std::vector<double>& shifts) {
  std::string s;
  for (std::size_t i = 0; i < shifts.size(); ++i) {
    if (shifts[i] == 0.0) {
      s += R"({"id":"b)" + std::to_string(i) + R"(","type":"Identity"},)";
    } else {
      s += R"({"id":"b)" + std::to_string(i) +
           R"(","type":"Add","value":)" + std::to_string(shifts[i]) + "},";
    }
  }
  return s;
}

std::string branch_connections_json(std::size_t n) {
  std::string s;
  for (std::size_t i = 0; i < n; ++i) {
    s += R"({"from":"in","to":"b)" + std::to_string(i) +
         R"(","fromPort":"o1","toPort":"i1"},)";
  }
  return s;
}

std::string make_topk_via_vc_json(std::size_t row_w,
                                   const std::vector<double>& shifts,
                                   int k, int score_index, bool descending) {
  std::string j;
  j  = R"({"title":"tk","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  j += branches_json(shifts);
  j += R"({"id":"vc","type":"VectorCompose","numPorts":)" +
       std::to_string(row_w) + R"(},)";
  j += R"({"id":"tk","type":"TopK","k":)" + std::to_string(k) +
       R"(,"score_index":)" + std::to_string(score_index) +
       R"(,"descending":)" + (descending ? "true" : "false") + R"(},)";
  j += R"({"id":"out","type":"Output","portTypes":["number"],"portWidths":[)" +
       std::to_string(row_w) + R"(]}],)";
  j += R"("connections":[)";
  j += branch_connections_json(row_w);
  for (std::size_t i = 0; i < row_w; ++i) {
    j += R"({"from":"b)" + std::to_string(i) +
         R"(","to":"vc","fromPort":"o1","toPort":"i)" +
         std::to_string(i + 1) + R"("},)";
  }
  j += R"({"from":"vc","to":"tk","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"tk","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

// Scalar variant: Input -> TopK(k, score_index=0) -> Output (width 1).
std::string make_topk_scalar_json(int k, bool descending) {
  std::string j;
  j  = R"({"title":"tks","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":["o1"]},)";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  j += R"({"id":"tk","type":"TopK","k":)" + std::to_string(k) +
       R"(,"score_index":0,"descending":)" +
       (descending ? "true" : "false") + R"(},)";
  j += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  j += R"("connections":[)";
  j += R"({"from":"in","to":"tk","fromPort":"o1","toPort":"i1"},)";
  j += R"({"from":"tk","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

// ----- Drivers --------------------------------------------------------------

std::vector<EmitRow> run_jit(const std::string& json,
                              const std::vector<Sample>& inputs) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  for (const auto& s : inputs) prog->receive(s.t, s.v);
  std::vector<EmitRow> out;
  for (const auto& r : prog->collect_outputs()) {
    out.push_back({r.time, r.values});
  }
  return out;
}

// Drive an FE TopK directly with synthetic VectorNumberData rows. shifts
// determine how each input scalar is fanned out across the row's lanes
// (lane k = v + shifts[k]). Mirrors the JIT pipeline exactly.
std::vector<EmitRow> run_fe_topk(const std::vector<double>& shifts,
                                  int k, int score_index, bool descending,
                                  const std::vector<Sample>& inputs) {
  auto topk = rtbot::make_topk("tk", k, score_index, descending);
  auto col = std::make_shared<rtbot::Collector>(
      "c", std::vector<std::string>{"vector_number"});
  topk->connect(col, 0, 0);

  for (const auto& s : inputs) {
    std::vector<double> row(shifts.size());
    for (std::size_t i = 0; i < shifts.size(); ++i) {
      row[i] = s.v + shifts[i];
    }
    topk->receive_data(
        rtbot::create_message<rtbot::VectorNumberData>(
            s.t, rtbot::VectorNumberData{row}), 0);
  }
  topk->execute();

  std::vector<EmitRow> out;
  auto& q = col->get_data_queue(0);
  for (auto& msg_ptr : q) {
    const auto* msg = static_cast<const rtbot::Message<rtbot::VectorNumberData>*>(
        msg_ptr.get());
    out.push_back({msg->time, *msg->data.values});
  }
  return out;
}

void assert_match(const std::vector<EmitRow>& a,
                   const std::vector<EmitRow>& b) {
  REQUIRE(a.size() == b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    INFO("rec " << i << " a.t=" << a[i].t << " b.t=" << b[i].t
                << " a.size=" << a[i].values.size()
                << " b.size=" << b[i].values.size());
    REQUIRE(a[i].t == b[i].t);
    REQUIRE(a[i].values.size() == b[i].values.size());
    for (std::size_t k = 0; k < a[i].values.size(); ++k) {
      INFO("  lane " << k << " a=" << a[i].values[k]
                     << " b=" << b[i].values[k]);
      REQUIRE(dbits(a[i].values[k]) == dbits(b[i].values[k]));
    }
  }
}

}  // namespace

SCENARIO("TopK FE-vs-JIT parity, K=3 N=2 (id, score) descending",
         "[jit][topk][parity]") {
  // shifts[0] = 0.0  (lane 0 = v, treated as id)
  // shifts[1] = 7.5  (lane 1 = v + 7.5, the score)
  std::vector<double> shifts{0.0, 7.5};
  const int K = 3;
  const int score_index = 1;
  const bool descending = true;

  std::vector<Sample> inputs;
  std::mt19937_64 rng(0xC0DEFEEDULL);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  for (std::int64_t i = 1; i <= 10; ++i) {
    inputs.push_back({i, dist(rng)});
  }

  auto jit_out = run_jit(
      make_topk_via_vc_json(shifts.size(), shifts, K, score_index, descending),
      inputs);
  auto fe_out  = run_fe_topk(shifts, K, score_index, descending, inputs);

  // Sanity: count grows 1, 2, 3, 3, 3, ...
  REQUIRE(fe_out.size() == 1 + 2 + 3 + 7 * 3);
  assert_match(jit_out, fe_out);
}

SCENARIO("TopK FE-vs-JIT parity, K=5 N=1 scalar inputs descending",
         "[jit][topk][parity]") {
  const int K = 5;
  const bool descending = true;

  std::vector<Sample> inputs;
  std::mt19937_64 rng(0xBADD0FF1ULL);
  std::uniform_real_distribution<double> dist(-50.0, 50.0);
  for (std::int64_t i = 1; i <= 20; ++i) {
    inputs.push_back({i, dist(rng)});
  }

  // JIT: scalar pipeline.
  auto jit_out = run_jit(make_topk_scalar_json(K, descending), inputs);
  // FE: shifts = {0.0} so each row is a 1-lane vector with the scalar value.
  std::vector<double> shifts{0.0};
  auto fe_out  = run_fe_topk(shifts, K, /*score_index=*/0, descending, inputs);

  REQUIRE(fe_out.size() == 1 + 2 + 3 + 4 + 16 * 5);
  assert_match(jit_out, fe_out);
}

SCENARIO("TopK FE-vs-JIT parity, K=3 N=2 with deliberate score ties",
         "[jit][topk][parity]") {
  // Drive a stream where many scores collide. shifts[1] = 0.0 means lane 1 is
  // exactly v, so when the input scalar v repeats, the score repeats too.
  std::vector<double> shifts{1.0, 0.0};
  const int K = 3;
  const int score_index = 1;
  const bool descending = true;

  std::vector<Sample> inputs{
      {1, 100.0}, {2, 100.0}, {3, 50.0},  {4, 100.0}, {5, 75.0},
      {6, 100.0}, {7, 100.0}, {8, 200.0}, {9, 200.0}, {10, 50.0},
  };

  auto jit_out = run_jit(
      make_topk_via_vc_json(shifts.size(), shifts, K, score_index, descending),
      inputs);
  auto fe_out  = run_fe_topk(shifts, K, score_index, descending, inputs);

  assert_match(jit_out, fe_out);
}

SCENARIO("TopK FE-vs-JIT parity, K=4 N=1 ascending",
         "[jit][topk][parity]") {
  const int K = 4;
  const bool descending = false;

  std::vector<Sample> inputs;
  std::mt19937_64 rng(0xFEEDFACEULL);
  std::uniform_real_distribution<double> dist(-10.0, 10.0);
  for (std::int64_t i = 1; i <= 15; ++i) {
    inputs.push_back({i, dist(rng)});
  }

  auto jit_out = run_jit(make_topk_scalar_json(K, descending), inputs);
  std::vector<double> shifts{0.0};
  auto fe_out  = run_fe_topk(shifts, K, /*score_index=*/0, descending, inputs);

  assert_match(jit_out, fe_out);
}
