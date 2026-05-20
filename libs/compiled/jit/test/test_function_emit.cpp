// test_function_emit.cpp
//
// Bit-exact parity tests for the JIT IR emitter for the Function operator
// (RB-491).
//
// The Function operator is a stateless 1->1 piecewise interpolator with a
// fixed point table. Two interpolation modes are supported: LINEAR and
// HERMITE. The JIT emitter bakes the point table (and pre-computed tangents
// for Hermite) into the module as private global constant arrays and emits
// the same FP arithmetic order as the FE interpreter, so outputs must be
// bit-exact equal.

#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Message.h"
#include "rtbot/Operator.h"
#include "rtbot/PortType.h"
#include "rtbot/std/Function.h"
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

std::vector<Emit> drain_collector(rtbot::Collector& sink) {
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

std::string serialize_points(
    const std::vector<std::pair<double, double>>& pts) {
  std::string out = "[";
  for (std::size_t i = 0; i < pts.size(); ++i) {
    if (i) out += ",";
    out += "[" + std::to_string(pts[i].first) + "," +
           std::to_string(pts[i].second) + "]";
  }
  out += "]";
  return out;
}

std::string make_function_json(
    const std::vector<std::pair<double, double>>& points,
    bool use_hermite) {
  std::string json;
  json  = R"({"title":"function-parity","apiVersion":"v1","entryOperator":"in",)";
  json += R"("output":{"out":["o1"]},"operators":[)";
  json += R"({"id":"in","type":"Input","portTypes":["number"]},)";
  json += R"({"id":"fn","type":"Function","points":)" +
          serialize_points(points);
  json += R"(,"interpolation_type":")";
  json += (use_hermite ? "HERMITE" : "LINEAR");
  json += R"("},)";
  json += R"({"id":"out","type":"Output","portTypes":["number"]}],)";
  json += R"("connections":[)";
  json += R"({"from":"in","to":"fn","fromPort":"o1","toPort":"i1"},)";
  json += R"({"from":"fn","to":"out","fromPort":"o1","toPort":"i1"}]})";
  return json;
}

std::vector<Emit> run_fe_function(
    const std::vector<std::pair<double, double>>& points,
    bool use_hermite,
    const std::vector<Sample>& inputs) {
  auto fn = std::make_shared<rtbot::Function>(
      "fn", points,
      use_hermite ? rtbot::InterpolationType::HERMITE
                  : rtbot::InterpolationType::LINEAR);
  auto sink = std::make_shared<rtbot::Collector>(
      "sink", std::vector<std::string>{rtbot::PortType::NUMBER});
  fn->connect(sink, 0, 0);
  for (const auto& s : inputs) {
    fn->receive_data(rtbot::create_message<rtbot::NumberData>(
                          s.t, rtbot::NumberData{s.v}),
                      0);
  }
  fn->execute();
  return drain_collector(*sink);
}

// Non-uniform 6-point table — covers below-leftmost, on-knot, between knots,
// and above-rightmost x values.
std::vector<std::pair<double, double>> make_table() {
  return {
      {-2.5,  3.0},
      {-1.0,  0.5},
      { 0.0, -1.0},
      { 1.5,  2.25},
      { 4.0,  5.0},
      { 7.5,  0.0},
  };
}

// Hand-picked input x values exercising every branch:
//   - left extrapolation (x < xs[0])
//   - exactly on each knot (x == xs[k])
//   - between every consecutive pair of knots
//   - right extrapolation (x > xs[N-1])
std::vector<Sample> make_inputs(const std::vector<std::pair<double, double>>& pts) {
  std::vector<Sample> inputs;
  std::int64_t t = 1;

  // Left extrapolation values.
  for (double x : {-10.0, -5.0, -3.0}) {
    inputs.push_back({t++, x});
  }
  // Each knot.
  for (const auto& p : pts) {
    inputs.push_back({t++, p.first});
  }
  // Midpoints between consecutive knots.
  for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
    double mid = 0.5 * (pts[i].first + pts[i + 1].first);
    inputs.push_back({t++, mid});
  }
  // Random x values in [-12, 12] — covers everything plus right-extrap.
  std::mt19937_64 rng(0xFEEDFACEULL);
  std::uniform_real_distribution<double> dist(-12.0, 12.0);
  for (int i = 0; i < 100; ++i) {
    inputs.push_back({t++, dist(rng)});
  }
  // Explicit right-extrapolation values.
  for (double x : {8.0, 15.0, 100.0}) {
    inputs.push_back({t++, x});
  }
  return inputs;
}

}  // namespace

SCENARIO("JIT Function (LINEAR) matches FE bit-exactly",
         "[function][linear][parity]") {
  auto pts = make_table();
  auto inputs = make_inputs(pts);
  auto json   = make_function_json(pts, /*use_hermite=*/false);

  auto jit_out = run_jit(json, inputs);
  auto fe_out  = run_fe_function(pts, /*use_hermite=*/false, inputs);
  require_parity(jit_out, fe_out);
}

SCENARIO("JIT Function (HERMITE) matches FE bit-exactly",
         "[function][hermite][parity]") {
  auto pts = make_table();
  auto inputs = make_inputs(pts);
  auto json   = make_function_json(pts, /*use_hermite=*/true);

  auto jit_out = run_jit(json, inputs);
  auto fe_out  = run_fe_function(pts, /*use_hermite=*/true, inputs);
  require_parity(jit_out, fe_out);
}

SCENARIO("JIT Function handles minimal 2-point table",
         "[function][parity]") {
  std::vector<std::pair<double, double>> pts = {{0.0, 0.0}, {10.0, 100.0}};
  std::vector<Sample> inputs;
  std::int64_t t = 1;
  for (double x : {-5.0, 0.0, 2.5, 5.0, 7.5, 10.0, 15.0, 20.0}) {
    inputs.push_back({t++, x});
  }

  for (bool herm : {false, true}) {
    INFO("hermite=" << herm);
    auto json    = make_function_json(pts, herm);
    auto jit_out = run_jit(json, inputs);
    auto fe_out  = run_fe_function(pts, herm, inputs);
    require_parity(jit_out, fe_out);
  }
}
