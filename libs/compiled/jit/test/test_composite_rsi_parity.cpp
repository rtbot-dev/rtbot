#include <catch2/catch.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Program.h"
#include "rtbot/compiled/jit/JitCompiler.h"

namespace {

inline std::uint64_t dbits(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof(double));
  return b;
}

std::vector<double> random_walk(std::size_t n, std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::normal_distribution<double> step(0.0, 1.0);
  std::vector<double> v(n);
  double x = 100.0;
  for (auto& y : v) { x += step(rng); y = x; }
  return v;
}

// Composite form: a single "RelativeStrengthIndex" op that the JIT JsonParser
// flattens into the textbook primitive sub-graph.
std::string rsi_composite_json(std::size_t n) {
  return R"({
    "title": "RSI composite",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",  "type": "Input",  "portTypes": ["number"] },
      { "id": "rsi", "type": "RelativeStrengthIndex", "window_size": )" +
         std::to_string(n) + R"( },
      { "id": "out", "type": "Output", "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in",  "to": "rsi", "fromPort": "o1", "toPort": "i1" },
      { "from": "rsi", "to": "out", "fromPort": "o1", "toPort": "i1" }
    ]
  })";
}

// Primitive form: explicit sub-graph that mirrors the textbook RSI exactly.
// Matches expand_rsi() in JsonParser.cpp:
//   diff   = Difference(input)
//   gains  = LessThanOrEqualToReplace(diff, 0, 0)
//   dneg   = Scale(diff, -1)
//   losses = LessThanOrEqualToReplace(dneg, 0, 0)
//   sg     = MovingSum(gains, n)
//   sl     = MovingSum(losses, n)
//   rs     = sg / sl
//   rsi    = 100 - 100 / (1 + rs) implemented as Add(1) -> Power(-1) -> Scale(-100) -> Add(100)
std::string rsi_primitive_json(std::size_t n) {
  return R"({
    "title": "RSI primitives",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",          "type": "Input",                       "portTypes": ["number"] },
      { "id": "diff",        "type": "Difference" },
      { "id": "gains_pos",   "type": "LessThanOrEqualToReplace",    "value": 0.0, "replaceBy": 0.0 },
      { "id": "diff_neg",    "type": "Scale",                       "value": -1.0 },
      { "id": "losses_pos",  "type": "LessThanOrEqualToReplace",    "value": 0.0, "replaceBy": 0.0 },
      { "id": "sum_gains",   "type": "MovingSum",                   "window_size": )" +
         std::to_string(n) + R"( },
      { "id": "sum_losses",  "type": "MovingSum",                   "window_size": )" +
         std::to_string(n) + R"( },
      { "id": "rs",          "type": "Division",                    "numPorts": 2,
        "portTypes": ["number", "number"] },
      { "id": "one_plus_rs", "type": "Add",                         "value": 1.0 },
      { "id": "inv",         "type": "Power",                       "value": -1.0 },
      { "id": "scale_neg100","type": "Scale",                       "value": -100.0 },
      { "id": "rsi_out",     "type": "Add",                         "value": 100.0 },
      { "id": "out",         "type": "Output",                      "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in",           "to": "diff",        "fromPort": "o1", "toPort": "i1" },
      { "from": "diff",         "to": "gains_pos",   "fromPort": "o1", "toPort": "i1" },
      { "from": "diff",         "to": "diff_neg",    "fromPort": "o1", "toPort": "i1" },
      { "from": "diff_neg",     "to": "losses_pos",  "fromPort": "o1", "toPort": "i1" },
      { "from": "gains_pos",    "to": "sum_gains",   "fromPort": "o1", "toPort": "i1" },
      { "from": "losses_pos",   "to": "sum_losses",  "fromPort": "o1", "toPort": "i1" },
      { "from": "sum_gains",    "to": "rs",          "fromPort": "o1", "toPort": "i1" },
      { "from": "sum_losses",   "to": "rs",          "fromPort": "o1", "toPort": "i2" },
      { "from": "rs",           "to": "one_plus_rs", "fromPort": "o1", "toPort": "i1" },
      { "from": "one_plus_rs",  "to": "inv",         "fromPort": "o1", "toPort": "i1" },
      { "from": "inv",          "to": "scale_neg100","fromPort": "o1", "toPort": "i1" },
      { "from": "scale_neg100", "to": "rsi_out",     "fromPort": "o1", "toPort": "i1" },
      { "from": "rsi_out",      "to": "out",         "fromPort": "o1", "toPort": "i1" }
    ]
  })";
}

struct Sample { std::int64_t t; double v; };

std::vector<Sample> drive_jit(const std::string& json,
                              const std::vector<double>& prices) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  for (std::size_t i = 0; i < prices.size(); ++i) {
    prog->receive(static_cast<std::int64_t>(i + 1), prices[i]);
  }
  std::vector<Sample> out;
  for (const auto& r : prog->collect_outputs()) {
    out.push_back({r.time, r.values[0]});
  }
  return out;
}

std::vector<Sample> drive_fe(const std::string& json,
                             const std::vector<double>& prices) {
  using namespace rtbot;
  Program prog(json);
  std::vector<Sample> out;
  for (std::size_t i = 0; i < prices.size(); ++i) {
    auto batch = prog.receive(
        Message<NumberData>(static_cast<std::int64_t>(i + 1),
                            NumberData{prices[i]}));
    auto op_it = batch.find("out");
    if (op_it == batch.end()) continue;
    auto& op_batch = op_it->second;
    auto port_it = op_batch.find("o1");
    if (port_it == op_batch.end()) continue;
    for (const auto& msg_ptr : port_it->second) {
      const auto* m =
          static_cast<const Message<NumberData>*>(msg_ptr.get());
      out.push_back({m->time, m->data.value});
    }
  }
  return out;
}

}  // namespace

SCENARIO("Composite RelativeStrengthIndex flattens to a primitive sub-graph",
         "[rsi][composite][jit][parity]") {
  const std::size_t n = 14;
  const auto prices = random_walk(80, 0xBA516);

  const auto jit_composite = drive_jit(rsi_composite_json(n), prices);
  const auto jit_primitive = drive_jit(rsi_primitive_json(n), prices);
  const auto fe_primitive  = drive_fe(rsi_primitive_json(n), prices);

  // First emission must be at t = n (Diff with use_oldest_time labels the
  // first diff with t=1, so MovingSum's nth element reports out_t=n).
  // Without the two-phase fix, the parallel sum_gains / sum_losses branches
  // would desync during warmup and the first emission would land at t = 2n.
  REQUIRE(!jit_composite.empty());
  REQUIRE(jit_composite.front().t == static_cast<std::int64_t>(n));

  // Both JIT paths must produce the same sequence (composite expansion is
  // bit-identical to writing out the primitives by hand).
  REQUIRE(jit_composite.size() == jit_primitive.size());
  for (std::size_t i = 0; i < jit_composite.size(); ++i) {
    INFO("i=" << i << " t=" << jit_composite[i].t);
    REQUIRE(jit_composite[i].t == jit_primitive[i].t);
    REQUIRE(dbits(jit_composite[i].v) == dbits(jit_primitive[i].v));
  }

  // FE primitive pipeline must agree with the JIT bit-exactly: same Kahan
  // sums, same op order, same arithmetic.
  REQUIRE(jit_composite.size() == fe_primitive.size());
  for (std::size_t i = 0; i < jit_composite.size(); ++i) {
    INFO("i=" << i << " t=" << jit_composite[i].t
              << " jit=" << jit_composite[i].v
              << " fe="  << fe_primitive[i].v);
    REQUIRE(jit_composite[i].t == fe_primitive[i].t);
    REQUIRE(dbits(jit_composite[i].v) == dbits(fe_primitive[i].v));
  }
}
