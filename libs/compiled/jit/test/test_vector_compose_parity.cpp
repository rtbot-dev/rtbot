// test_vector_compose_parity.cpp
//
// Self-consistent parity tests for the JIT VectorCompose op.
//
// VectorCompose(N) syncs N scalar input ports and emits a single record whose
// output port carries N consecutive scalar slots. A reference pipeline mirrors
// the same input stream through N independent scalar branches that each feed a
// distinct width-1 input port on a single Output op.
//
// For every synced timestamp both pipelines must produce identical output:
//   - the same emit timestamp
//   - the same N output values, in the same slot order
//
// Why self-consistent rather than FE-vs-JIT: the FE Output op emits a single
// VectorNumberData message when its input port is "vector_number", whereas
// translate_jit_to_batch_ converts each scalar slot into an independent
// NumberData on its own o-port. The two ProgramMsgBatch shapes are not
// directly comparable without additional plumbing, so we instead drive the
// JIT via JitCompiler twice with semantically equivalent JSON programs and
// compare their EmittedRecord output bit-exact.

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "rtbot/compiled/jit/JitCompiler.h"
#include "rtbot/compiled/jit/JitCompiledProgram.h"

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

struct Sample {
  std::int64_t t;
  double v;
};

std::vector<Sample> make_inputs(std::uint64_t seed = 0xCAFED00DULL) {
  std::vector<Sample> out;
  out.reserve(100);
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  for (std::int64_t i = 1; i <= 100; ++i) {
    double v = dist(rng);
    if (i == 17) v = std::numeric_limits<double>::quiet_NaN();
    if (i == 42) v = std::numeric_limits<double>::infinity();
    if (i == 73) v = -std::numeric_limits<double>::infinity();
    out.push_back({i, v});
  }
  return out;
}

// Build N branch operators between Input and the downstream sync/output op.
// Branch i uses Add(shift_i) so the per-port values differ even though they
// share the same source Input. shift[i] == 0 turns into Identity to avoid the
// trivial 0.0 add.
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

// Pipeline A: Input -> N branches -> VectorCompose(N) -> Output (one port,
// width N).
std::string make_vc_json(std::size_t n_ports,
                          const std::vector<double>& shifts) {
  std::string out_ports = R"(["o1"])";
  std::string j;
  j  = R"({"title":"vc","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":)" + out_ports + "},";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  j += branches_json(shifts);
  j += R"({"id":"vc","type":"VectorCompose","numPorts":)" +
       std::to_string(n_ports) + R"(},)";
  j += R"({"id":"out","type":"Output","portTypes":["number"],"portWidths":[)" +
       std::to_string(n_ports) + R"(]}],)";
  j += R"("connections":[)";
  j += branch_connections_json(n_ports);
  for (std::size_t i = 0; i < n_ports; ++i) {
    j += R"({"from":"b)" + std::to_string(i) +
         R"(","to":"vc","fromPort":"o1","toPort":"i)" +
         std::to_string(i + 1) + R"("},)";
  }
  j += R"({"from":"vc","to":"out","fromPort":"o1","toPort":"i1"})";
  j += R"(]})";
  return j;
}

// Pipeline B reference: Input -> N branches; each branch feeds a separate
// width-1 input port on a single Output op. The Output emits one record per
// tick with the same N scalar values as Pipeline A's VectorCompose record.
//
// Output values are written by SegmentEmitter into out_v_arr in port-index
// order, so as long as branch i is wired to input port i+1 of the Output,
// slot k in the resulting EmittedRecord matches Pipeline A's slot k.
std::string make_ref_json(std::size_t n_ports,
                           const std::vector<double>& shifts) {
  std::string output_ports;
  output_ports = "[";
  for (std::size_t i = 0; i < n_ports; ++i) {
    if (i > 0) output_ports += ",";
    output_ports += "\"o" + std::to_string(i + 1) + "\"";
  }
  output_ports += "]";

  std::string port_types_arr;
  port_types_arr = "[";
  for (std::size_t i = 0; i < n_ports; ++i) {
    if (i > 0) port_types_arr += ",";
    port_types_arr += "\"number\"";
  }
  port_types_arr += "]";

  // For the reference, the Output op needs N branches feeding its N
  // distinct input ports. To force a single record per tick all N branches
  // must arrive synchronously, which they do because they share the Input.
  // However Output op alone does NOT sync — it emits per-port as values
  // arrive. To keep the reference single-record-per-tick we route all
  // branches through a Join(N) sync op first, then connect Join's N output
  // ports to Output's N input ports in order.
  std::string j;
  j  = R"({"title":"ref","apiVersion":"v1","entryOperator":"in",)";
  j += R"("output":{"out":)" + output_ports + "},";
  j += R"("operators":[)";
  j += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  j += branches_json(shifts);
  j += R"({"id":"jn","type":"Join","numPorts":)" +
       std::to_string(n_ports) + "," +
       R"("portTypes":)" + port_types_arr + R"(},)";
  j += R"({"id":"out","type":"Output","portTypes":)" + port_types_arr + R"(}],)";
  j += R"("connections":[)";
  j += branch_connections_json(n_ports);
  for (std::size_t i = 0; i < n_ports; ++i) {
    j += R"({"from":"b)" + std::to_string(i) +
         R"(","to":"jn","fromPort":"o1","toPort":"i)" +
         std::to_string(i + 1) + R"("},)";
    j += R"({"from":"jn","to":"out","fromPort":"o)" + std::to_string(i + 1) +
         R"(","toPort":"i)" + std::to_string(i + 1) + R"("},)";
  }
  // Trim trailing comma added by the loop's final entry.
  if (j.back() == ',') j.pop_back();
  j += R"(]})";
  return j;
}

struct EmitVec {
  std::int64_t t;
  std::vector<double> values;
};

std::vector<EmitVec> run_jit_and_collect(const std::string& json,
                                          const std::vector<Sample>& inputs) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  for (const auto& s : inputs) prog->receive(s.t, s.v);
  std::vector<EmitVec> out;
  for (const auto& r : prog->collect_outputs()) {
    out.push_back({r.time, r.values});
  }
  return out;
}

void assert_records_match(const std::vector<EmitVec>& a,
                           const std::vector<EmitVec>& b) {
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

TEST_CASE("VectorCompose JIT N=2 self-consistent parity",
          "[jit][vector_compose][parity]") {
  const std::size_t N = 2;
  std::vector<double> shifts{0.0, 1.5};
  auto vc_json  = make_vc_json(N, shifts);
  auto ref_json = make_ref_json(N, shifts);
  const auto inputs = make_inputs();

  auto vc_out  = run_jit_and_collect(vc_json,  inputs);
  auto ref_out = run_jit_and_collect(ref_json, inputs);

  REQUIRE_FALSE(vc_out.empty());
  // Each Pipeline A record has N values; Pipeline B emits one record per
  // tick with the same N values (one per Join output port -> one Output port).
  REQUIRE(vc_out.front().values.size() == N);
  REQUIRE(ref_out.front().values.size() == N);
  assert_records_match(vc_out, ref_out);
}

TEST_CASE("VectorCompose JIT N=3 self-consistent parity",
          "[jit][vector_compose][parity]") {
  const std::size_t N = 3;
  std::vector<double> shifts{0.0, -2.5, 7.25};
  auto vc_json  = make_vc_json(N, shifts);
  auto ref_json = make_ref_json(N, shifts);
  const auto inputs = make_inputs(0xBADC0FFEEULL);

  auto vc_out  = run_jit_and_collect(vc_json,  inputs);
  auto ref_out = run_jit_and_collect(ref_json, inputs);

  REQUIRE_FALSE(vc_out.empty());
  REQUIRE(vc_out.front().values.size() == N);
  REQUIRE(ref_out.front().values.size() == N);
  assert_records_match(vc_out, ref_out);
}
