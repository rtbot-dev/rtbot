// test_linear_reduce_parity.cpp
//
// Bit-exact parity tests for the JIT N-port sync ops:
//   - Linear (any N >= 2)
//   - ReduceJoin family for N >= 3 ports: Addition, Subtraction,
//     Multiplication, Division, LogicalAnd, LogicalOr, LogicalXor,
//     LogicalNand, LogicalNor, LogicalXnor, LogicalImplication
//
// Each test drives the same input stream through the FE interpreter pipeline
// and the JIT-compiled program, then asserts bit-exact (time, value) parity.
//
// Test pipeline shape:
//   Input -> Identity (port-1 stream)
//   Input -> Add(k_2)  (port-2 stream)
//   Input -> Add(k_3)  (port-3 stream)
//   ...
//   each branch -> the N-port sync op -> Output
//
// All branches share the same source timestamps so all ports tick together.

#include <catch2/catch.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"
#include "rtbot/std/ArithmeticScalar.h"
#include "rtbot/std/ArithmeticSync.h"
#include "rtbot/std/BooleanSync.h"
#include "rtbot/std/BooleanToNumber.h"
#include "rtbot/std/Identity.h"
#include "rtbot/std/Linear.h"
#include "rtbot/compiled/jit/JitCompiler.h"

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

struct Emit {
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

std::vector<Sample> make_positive_inputs(std::uint64_t seed = 0xC0DECAFEULL) {
  std::vector<Sample> out;
  out.reserve(100);
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> dist(0.5, 50.0);
  for (std::int64_t i = 1; i <= 100; ++i) {
    out.push_back({i, dist(rng)});
  }
  return out;
}

std::vector<Emit> run_jit(const std::string& json,
                          const std::vector<Sample>& inputs) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  for (const auto& s : inputs) prog->receive(s.t, s.v);
  std::vector<Emit> out;
  for (const auto& r : prog->collect_outputs()) {
    REQUIRE(r.values.size() == 1);
    out.push_back({r.time, r.values[0]});
  }
  return out;
}

std::vector<Emit> drain_collector_number(rtbot::Collector& sink) {
  std::vector<Emit> out;
  auto& q = sink.get_data_queue(0);
  for (auto& msg_ptr : q) {
    const auto* msg =
        static_cast<const rtbot::Message<rtbot::NumberData>*>(msg_ptr.get());
    out.push_back({msg->time, msg->data.value});
  }
  q.clear();
  return out;
}

std::vector<Emit> drain_collector_bool_as_number(rtbot::Collector& sink) {
  std::vector<Emit> out;
  auto& q = sink.get_data_queue(0);
  for (auto& msg_ptr : q) {
    const auto* msg =
        static_cast<const rtbot::Message<rtbot::NumberData>*>(msg_ptr.get());
    out.push_back({msg->time, msg->data.value});
  }
  q.clear();
  return out;
}

void require_parity(const std::vector<Emit>& jit_out,
                    const std::vector<Emit>& fe_out) {
  REQUIRE(jit_out.size() == fe_out.size());
  for (std::size_t i = 0; i < jit_out.size(); ++i) {
    INFO("emit index " << i
         << " jit=(" << jit_out[i].t << ", " << jit_out[i].v << ")"
         << " fe=("  << fe_out[i].t  << ", " << fe_out[i].v  << ")");
    REQUIRE(jit_out[i].t == fe_out[i].t);
    REQUIRE(dbits(jit_out[i].v) == dbits(fe_out[i].v));
  }
}

// Build a JSON pipeline:
//   Input -> [N branches: branch[0] = Identity, branch[i] = Add(k_i)]
//          -> sync_op(type) at ports i1..iN -> Output
// `op_extra` is appended into the sync op JSON object (e.g. "coefficients":[..]).
std::string make_n_port_sync_json(const std::string& op_type,
                                   std::size_t n_ports,
                                   const std::vector<double>& shifts,
                                   const std::string& op_extra) {
  REQUIRE(shifts.size() == n_ports);
  std::string json;
  json  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  json += R"("output":{"out":["o1"]},"operators":[)";
  json += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  for (std::size_t i = 0; i < n_ports; ++i) {
    if (shifts[i] == 0.0) {
      // Identity branch (avoids spurious Add(0.0) which still works but is noisy).
      json += R"({"id":"b)" + std::to_string(i) + R"(","type":"Identity"},)";
    } else {
      json += R"({"id":"b)" + std::to_string(i) +
              R"(","type":"Add","value":)" + std::to_string(shifts[i]) + "},";
    }
  }
  json += R"({"id":"op","type":")" + op_type + R"(")";
  if (!op_extra.empty()) json += "," + op_extra;
  json += "},";
  json += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  json += R"("connections":[)";
  for (std::size_t i = 0; i < n_ports; ++i) {
    json += R"({"from":"in","to":"b)" + std::to_string(i) +
            R"(","fromPort":"o1","toPort":"i1"},)";
    json += R"({"from":"b)" + std::to_string(i) +
            R"(","to":"op","fromPort":"o1","toPort":"i)" +
            std::to_string(i + 1) + R"("},)";
  }
  json += R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
  return json;
}

// Drive an FE op: feed the same input stream into N Add-shift branches and
// connect their outputs into the N-port FE op, then drain the collector.
std::vector<Emit> run_fe_n_port(
    const std::shared_ptr<rtbot::Operator>& fe_op,
    const std::vector<Sample>& inputs,
    const std::vector<double>& shifts) {
  const std::size_t n = shifts.size();
  std::vector<std::shared_ptr<rtbot::Operator>> branches;
  branches.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    if (shifts[i] == 0.0) {
      branches.push_back(rtbot::make_identity("b" + std::to_string(i)));
    } else {
      branches.push_back(rtbot::make_add(
          "b" + std::to_string(i), shifts[i]));
    }
    branches.back()->connect(fe_op, 0, i);
  }
  auto sink = rtbot::make_collector(
      "sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  fe_op->connect(sink, 0, 0);

  for (const auto& s : inputs) {
    for (std::size_t i = 0; i < n; ++i) {
      branches[i]->receive_data(
          rtbot::create_message<rtbot::NumberData>(s.t, rtbot::NumberData{s.v}),
          0);
    }
  }
  for (std::size_t i = 0; i < n; ++i) branches[i]->execute();
  return drain_collector_number(*sink);
}

// JSON for an N-port boolean reduce pipeline. Each branch is a CompareNEQ
// against 0.0 with tolerance 0.0, mapping the input to 0.0/1.0. The op
// result is wrapped in BooleanToNumber so the JIT pipeline emits doubles
// (0.0/1.0) for parity comparison. To produce N branches with different
// boolean values from a single Input, we additionally invert via "Sub from 1"
// for some branches to vary across ports while preserving timestamp sync.
std::string make_n_port_bool_sync_json(const std::string& op_type,
                                        std::size_t n_ports) {
  std::string json;
  json  = R"({"title":"t","apiVersion":"v1","entryOperator":"in",)";
  json += R"("output":{"out":["o1"]},"operators":[)";
  json += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  for (std::size_t i = 0; i < n_ports; ++i) {
    // branch: predicate that produces 0.0/1.0 deterministically for each port
    // by comparing input vs port-specific threshold. CompareNEQ with the
    // alternating threshold creates varied per-port booleans synced at t.
    double thr = (i % 2 == 0) ? 0.5 : -0.5;
    json += R"({"id":"b)" + std::to_string(i) +
            R"(","type":"CompareGT","value":)" + std::to_string(thr) + "},";
  }
  json += R"({"id":"op","type":")" + op_type +
          R"(","numPorts":)" + std::to_string(n_ports) + R"(},)";
  json += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  json += R"("connections":[)";
  for (std::size_t i = 0; i < n_ports; ++i) {
    json += R"({"from":"in","to":"b)" + std::to_string(i) +
            R"(","fromPort":"o1","toPort":"i1"},)";
    json += R"({"from":"b)" + std::to_string(i) +
            R"(","to":"op","fromPort":"o1","toPort":"i)" +
            std::to_string(i + 1) + R"("},)";
  }
  json += R"({"from":"op","to":"out","fromPort":"o1","toPort":"i1"}]})";
  return json;
}

// FE driver for boolean N-port reduce mirroring the JSON above:
// each branch is CompareGT vs alternating thresholds, producing
// BooleanData. Drive the FE op directly via its data ports.
std::vector<Emit> run_fe_bool_pipeline(
    const std::shared_ptr<rtbot::Operator>& fe_op,
    const std::vector<Sample>& inputs,
    std::size_t n) {
  // We synthesize the per-port boolean values inline (matching the JSON
  // CompareGT semantics) because the FE's CompareScalar emits BooleanData
  // and the wiring to a BooleanSync requires routing each branch's output
  // to the corresponding port. We bypass the wiring overhead by computing
  // the per-port boolean directly here.
  auto sink = rtbot::make_collector(
      "sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  auto b2n = rtbot::make_boolean_to_number("b2n");
  fe_op->connect(b2n, 0, 0);
  b2n->connect(sink, 0, 0);

  for (const auto& s : inputs) {
    for (std::size_t i = 0; i < n; ++i) {
      double thr = (i % 2 == 0) ? 0.5 : -0.5;
      bool bv = s.v > thr;
      fe_op->receive_data(
          rtbot::create_message<rtbot::BooleanData>(
              s.t, rtbot::BooleanData{bv}),
          i);
    }
  }
  fe_op->execute();
  return drain_collector_number(*sink);
}

}  // namespace

// ---------------------------------------------------------------------------
// Linear (N = 2, 3, 5)
// ---------------------------------------------------------------------------
SCENARIO("JIT Linear matches FE bit-exactly across N=2,3,5",
         "[linear][parity]") {
  auto inputs = make_inputs();

  struct Case {
    std::vector<double> coeffs;
    std::vector<double> shifts;
  };
  // Coefficients include positive, negative, fractional, and zero values.
  std::vector<Case> cases = {
    { { 1.5, -0.5 },                       {  0.0, 7.25 } },
    { { 1.0,  2.0,  3.0 },                 {  0.0, 1.0, -3.0 } },
    { {-1.25, 0.5, 0.0, 2.0,  -0.75 },     {  0.0, 4.5, 9.0, -2.0, 11.0 } },
  };

  for (const auto& c : cases) {
    INFO("Linear N=" << c.coeffs.size());
    // Build coefficients JSON literal.
    std::string coeffs_json = "\"coefficients\":[";
    for (std::size_t i = 0; i < c.coeffs.size(); ++i) {
      if (i) coeffs_json += ",";
      coeffs_json += std::to_string(c.coeffs[i]);
    }
    coeffs_json += "]";
    std::string json = make_n_port_sync_json(
        "Linear", c.coeffs.size(), c.shifts, coeffs_json);
    auto jit_out = run_jit(json, inputs);

    auto fe_op = rtbot::make_linear("op", c.coeffs);
    auto fe_out = run_fe_n_port(
        std::static_pointer_cast<rtbot::Operator>(fe_op), inputs, c.shifts);

    require_parity(jit_out, fe_out);
  }
}

// ---------------------------------------------------------------------------
// ReduceJoin: arithmetic family, N >= 3
// ---------------------------------------------------------------------------
SCENARIO("JIT Addition reduce N=3 matches FE bit-exactly",
         "[reduce_join][addition][parity]") {
  auto inputs = make_inputs();
  std::vector<double> shifts = { 0.0, 7.25, -3.5 };
  std::string json =
      make_n_port_sync_json("Addition", 3, shifts, "\"numPorts\":3");
  auto jit_out = run_jit(json, inputs);

  auto fe_op = rtbot::make_addition("op", 3);
  auto fe_out = run_fe_n_port(
      std::static_pointer_cast<rtbot::Operator>(fe_op), inputs, shifts);
  require_parity(jit_out, fe_out);
}

SCENARIO("JIT Subtraction reduce N=3 matches FE bit-exactly",
         "[reduce_join][subtraction][parity]") {
  auto inputs = make_inputs();
  std::vector<double> shifts = { 0.0, 1.0, 2.5 };
  std::string json =
      make_n_port_sync_json("Subtraction", 3, shifts, "\"numPorts\":3");
  auto jit_out = run_jit(json, inputs);

  auto fe_op = rtbot::make_subtraction("op", 3);
  auto fe_out = run_fe_n_port(
      std::static_pointer_cast<rtbot::Operator>(fe_op), inputs, shifts);
  require_parity(jit_out, fe_out);
}

SCENARIO("JIT Multiplication reduce N=4 matches FE bit-exactly",
         "[reduce_join][multiplication][parity]") {
  auto inputs = make_inputs();
  std::vector<double> shifts = { 0.0, 1.5, 3.0, -0.25 };
  std::string json =
      make_n_port_sync_json("Multiplication", 4, shifts, "\"numPorts\":4");
  auto jit_out = run_jit(json, inputs);

  auto fe_op = rtbot::make_multiplication("op", 4);
  auto fe_out = run_fe_n_port(
      std::static_pointer_cast<rtbot::Operator>(fe_op), inputs, shifts);
  require_parity(jit_out, fe_out);
}

SCENARIO("JIT Division reduce N=3 matches FE bit-exactly",
         "[reduce_join][division][parity]") {
  // Use positive inputs and positive shifts so divisors are non-zero and
  // FE / JIT both emit on every tick.
  auto inputs = make_positive_inputs();
  std::vector<double> shifts = { 0.0, 1.5, 3.0 };
  std::string json =
      make_n_port_sync_json("Division", 3, shifts, "\"numPorts\":3");
  auto jit_out = run_jit(json, inputs);

  auto fe_op = rtbot::make_division("op", 3);
  auto fe_out = run_fe_n_port(
      std::static_pointer_cast<rtbot::Operator>(fe_op), inputs, shifts);
  require_parity(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// ReduceJoin: boolean family, N >= 3
// ---------------------------------------------------------------------------
SCENARIO("JIT LogicalAnd/Or/Xor/Nand/Nor/Xnor/Implication N=3 match FE bit-exactly",
         "[reduce_join][boolean][parity]") {
  auto inputs = make_inputs();
  const std::size_t N = 3;

  struct BoolCase {
    const char* type;
    std::shared_ptr<rtbot::Operator> (*make)(std::string, std::size_t);
  };
  BoolCase cases[] = {
    {"LogicalAnd",         [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_and(std::move(id), n)); }},
    {"LogicalOr",          [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_or(std::move(id), n)); }},
    {"LogicalXor",         [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_xor(std::move(id), n)); }},
    {"LogicalNand",        [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_nand(std::move(id), n)); }},
    {"LogicalNor",         [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_nor(std::move(id), n)); }},
    {"LogicalXnor",        [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_xnor(std::move(id), n)); }},
    {"LogicalImplication", [](std::string id, std::size_t n){ return std::shared_ptr<rtbot::Operator>(rtbot::make_logical_implication(std::move(id), n)); }},
  };

  for (const auto& c : cases) {
    INFO("Boolean reduce N=3 type=" << c.type);
    std::string json = make_n_port_bool_sync_json(c.type, N);
    auto jit_out = run_jit(json, inputs);

    auto fe_op = c.make("op", N);
    auto fe_out = run_fe_bool_pipeline(fe_op, inputs, N);
    require_parity(jit_out, fe_out);
  }
}

// ---------------------------------------------------------------------------
// Sanity: JIT 2-port Addition (legacy path via OpKind::Add) still matches FE
// 2-port Addition bit-exactly. This guards against accidentally re-routing
// the 2-port case through the new ReduceJoin path.
// ---------------------------------------------------------------------------
SCENARIO("JIT Addition N=2 (legacy stateless) matches FE bit-exactly",
         "[reduce_join][addition][n2][parity]") {
  auto inputs = make_inputs();
  std::vector<double> shifts = { 0.0, 5.5 };
  // No numPorts -> stays on legacy stateless 2-input path.
  std::string json = make_n_port_sync_json("Addition", 2, shifts, "");
  auto jit_out = run_jit(json, inputs);

  auto fe_op = rtbot::make_addition("op", 2);
  auto fe_out = run_fe_n_port(
      std::static_pointer_cast<rtbot::Operator>(fe_op), inputs, shifts);
  require_parity(jit_out, fe_out);
}
