// test_demux_mux_parity.cpp
//
// Bit-exact parity tests for the Demultiplexer and Multiplexer JIT IR
// emitters added in RB-491 / D3.
//
// The JIT graphs feed a single Input through stateless predicates to derive
// the data + boolean control wires, so the entire pipeline collapses into one
// emit_program function with a Demux/Mux sync op at the end.
//
// FE side: drives the same logical inputs through a hand-built Program (which
// uses the FE interpreter for these graphs since the equivalent JSON exercises
// the same operators).

#include <catch2/catch.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "rtbot/Message.h"
#include "rtbot/Program.h"
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

// Drive the JIT program with a sequence of (t, v) samples and collect emits
// keyed by (port_id, time, value). port_id is 0-based.
struct DemuxEmit {
  std::int64_t t;
  std::int32_t port_id;
  double v;
};

std::vector<DemuxEmit> run_jit_demux(const std::string& json,
                                      const std::vector<Sample>& inputs) {
  rtbot::jit::JitCompiler compiler;
  auto prog = compiler.compile(json);
  REQUIRE(prog != nullptr);
  for (const auto& s : inputs) prog->receive(s.t, s.v);
  std::vector<DemuxEmit> out;
  for (const auto& r : prog->collect_outputs()) {
    REQUIRE(r.values.size() >= 1);
    out.push_back({r.time, r.port_id, r.values[0]});
  }
  return out;
}

// Drive the same JSON through the FE interpreter (Program::receive with
// unique_ptr<BaseMessage>) and collect emits as (t, port_id, v).
std::vector<DemuxEmit> run_fe(const std::string& json,
                               const std::vector<Sample>& inputs) {
  rtbot::Program prog(json);
  std::vector<DemuxEmit> out;
  for (const auto& s : inputs) {
    auto batch = prog.receive(rtbot::create_message<rtbot::NumberData>(
        s.t, rtbot::NumberData{s.v}), "i1");
    // batch is a map<op_id, map<port_name, port_msgs>>. We expect a single
    // op (the Output op). Iterate ports in numeric order o1, o2, o3, ...
    for (const auto& [op_id, op_batch] : batch) {
      for (const auto& [port_name, msgs] : op_batch) {
        // port_name is "o{k+1}" -> port_id k.
        REQUIRE(port_name.size() >= 2);
        REQUIRE(port_name[0] == 'o');
        std::int32_t pid = std::stoi(port_name.substr(1)) - 1;
        for (const auto& msg : msgs) {
          const auto* nm =
              dynamic_cast<const rtbot::Message<rtbot::NumberData>*>(msg.get());
          REQUIRE(nm != nullptr);
          out.push_back({nm->time, pid, nm->data.value});
        }
      }
    }
  }
  return out;
}

void require_demux_parity(std::vector<DemuxEmit> jit_out,
                           std::vector<DemuxEmit> fe_out) {
  // Sort both by (t, port_id) before comparing — the JIT and FE may emit in
  // a different order for multi-emit ticks.
  auto cmp = [](const DemuxEmit& a, const DemuxEmit& b) {
    if (a.t != b.t) return a.t < b.t;
    return a.port_id < b.port_id;
  };
  std::sort(jit_out.begin(), jit_out.end(), cmp);
  std::sort(fe_out.begin(), fe_out.end(), cmp);
  REQUIRE(jit_out.size() == fe_out.size());
  for (std::size_t i = 0; i < jit_out.size(); ++i) {
    INFO("emit index " << i
         << " jit=(t=" << jit_out[i].t << ", pid=" << jit_out[i].port_id
         << ", v=" << jit_out[i].v << ")"
         << " fe=(t=" << fe_out[i].t << ", pid=" << fe_out[i].port_id
         << ", v=" << fe_out[i].v << ")");
    REQUIRE(jit_out[i].t == fe_out[i].t);
    REQUIRE(jit_out[i].port_id == fe_out[i].port_id);
    REQUIRE(dbits(jit_out[i].v) == dbits(fe_out[i].v));
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Demultiplexer (numPorts=2). Routes data to o1 if c1==true, to o2 if c2==true.
// JIT graph derives data, c1, c2 from a single Input via:
//   data := Identity(in)
//   c1   := CompareGTE(in, 0.0)   (1.0 / 0.0)
//   c2   := CompareLT(in, 0.0)    (1.0 / 0.0)
// So at every tick exactly one of c1, c2 is true.
// ---------------------------------------------------------------------------
SCENARIO("JIT Demultiplexer N=2 routes by control predicate", "[demux][parity]") {
  const char* kJson = R"({
    "title": "Demux N=2",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1", "o2"] },
    "operators": [
      { "id": "in",   "type": "Input", "portTypes": ["number"] },
      { "id": "data", "type": "Identity" },
      { "id": "c1",   "type": "CompareGTE", "value": 0.0 },
      { "id": "c2",   "type": "CompareLT",  "value": 0.0 },
      { "id": "dmx",  "type": "Demultiplexer", "numPorts": 2 },
      { "id": "out",  "type": "Output", "portTypes": ["number", "number"] }
    ],
    "connections": [
      { "from": "in",   "to": "data", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "c1",   "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "c2",   "fromPort": "o1", "toPort": "i1" },
      { "from": "data", "to": "dmx",  "fromPort": "o1", "toPort": "i1" },
      { "from": "c1",   "to": "dmx",  "fromPort": "o1", "toPort": "c1" },
      { "from": "c2",   "to": "dmx",  "fromPort": "o1", "toPort": "c2" },
      { "from": "dmx",  "to": "out",  "fromPort": "o1", "toPort": "i1" },
      { "from": "dmx",  "to": "out",  "fromPort": "o2", "toPort": "i2" }
    ]
  })";

  std::vector<Sample> inputs = {
    {1,  3.5},  // c1=true: emit on port 0
    {2, -1.0},  // c2=true: emit on port 1
    {3,  0.0},  // c1=true: emit on port 0
    {4, -0.5},  // c2=true: emit on port 1
    {5,  7.0},  // c1=true: emit on port 0
  };

  auto jit_out = run_jit_demux(kJson, inputs);

  // Expected: each input drives exactly one output port.
  REQUIRE(jit_out.size() == inputs.size());

  REQUIRE(jit_out[0].t == 1); REQUIRE(jit_out[0].port_id == 0); REQUIRE(dbits(jit_out[0].v) == dbits(3.5));
  REQUIRE(jit_out[1].t == 2); REQUIRE(jit_out[1].port_id == 1); REQUIRE(dbits(jit_out[1].v) == dbits(-1.0));
  REQUIRE(jit_out[2].t == 3); REQUIRE(jit_out[2].port_id == 0); REQUIRE(dbits(jit_out[2].v) == dbits(0.0));
  REQUIRE(jit_out[3].t == 4); REQUIRE(jit_out[3].port_id == 1); REQUIRE(dbits(jit_out[3].v) == dbits(-0.5));
  REQUIRE(jit_out[4].t == 5); REQUIRE(jit_out[4].port_id == 0); REQUIRE(dbits(jit_out[4].v) == dbits(7.0));

  // FE parity.
  auto fe_out = run_fe(kJson, inputs);
  require_demux_parity(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Demultiplexer (numPorts=3). Route by sign category.
//   c1 := v > 0.5    -> port 0
//   c2 := |v| <= 0.5 -> port 1   (CompareSyncLTE, paired with constant)
//   c3 := v < -0.5   -> port 2
// ---------------------------------------------------------------------------
SCENARIO("JIT Demultiplexer N=3 routes by 3-way predicate", "[demux][parity]") {
  // For the 3-way test we use scalar comparisons so all controls are derived
  // stateless ops driven by Input. CompareGT, EQ-Range via two filters,
  // CompareLT.
  // c2 := v >= -0.5 AND v <= 0.5. Express as two scalar CompareLTE/GTE then
  // LogicalAnd. Using "LogicalAnd" (BooleanSync) requires both operand wires
  // to be aligned — they are, since they share the same upstream Input.
  const char* kJson = R"({
    "title": "Demux N=3",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1", "o2", "o3"] },
    "operators": [
      { "id": "in",   "type": "Input", "portTypes": ["number"] },
      { "id": "data", "type": "Identity" },
      { "id": "c1",   "type": "CompareGT",  "value": 0.5 },
      { "id": "c2a",  "type": "CompareGTE", "value": -0.5 },
      { "id": "c2b",  "type": "CompareLTE", "value": 0.5 },
      { "id": "c2",   "type": "LogicalAnd" },
      { "id": "c3",   "type": "CompareLT",  "value": -0.5 },
      { "id": "dmx",  "type": "Demultiplexer", "numPorts": 3 },
      { "id": "out",  "type": "Output", "portTypes": ["number", "number", "number"] }
    ],
    "connections": [
      { "from": "in",   "to": "data", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "c1",   "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "c2a",  "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "c2b",  "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "c3",   "fromPort": "o1", "toPort": "i1" },
      { "from": "c2a",  "to": "c2",   "fromPort": "o1", "toPort": "i1" },
      { "from": "c2b",  "to": "c2",   "fromPort": "o1", "toPort": "i2" },
      { "from": "data", "to": "dmx",  "fromPort": "o1", "toPort": "i1" },
      { "from": "c1",   "to": "dmx",  "fromPort": "o1", "toPort": "c1" },
      { "from": "c2",   "to": "dmx",  "fromPort": "o1", "toPort": "c2" },
      { "from": "c3",   "to": "dmx",  "fromPort": "o1", "toPort": "c3" },
      { "from": "dmx",  "to": "out",  "fromPort": "o1", "toPort": "i1" },
      { "from": "dmx",  "to": "out",  "fromPort": "o2", "toPort": "i2" },
      { "from": "dmx",  "to": "out",  "fromPort": "o3", "toPort": "i3" }
    ]
  })";

  std::vector<Sample> inputs = {
    {1,  1.0},  // port 0 (c1)
    {2,  0.0},  // port 1 (c2)
    {3, -1.0},  // port 2 (c3)
    {4,  0.4},  // port 1 (c2)
    {5,  2.0},  // port 0 (c1)
  };

  auto jit_out = run_jit_demux(kJson, inputs);

  REQUIRE(jit_out.size() == inputs.size());
  REQUIRE(jit_out[0].t == 1); REQUIRE(jit_out[0].port_id == 0); REQUIRE(dbits(jit_out[0].v) == dbits(1.0));
  REQUIRE(jit_out[1].t == 2); REQUIRE(jit_out[1].port_id == 1); REQUIRE(dbits(jit_out[1].v) == dbits(0.0));
  REQUIRE(jit_out[2].t == 3); REQUIRE(jit_out[2].port_id == 2); REQUIRE(dbits(jit_out[2].v) == dbits(-1.0));
  REQUIRE(jit_out[3].t == 4); REQUIRE(jit_out[3].port_id == 1); REQUIRE(dbits(jit_out[3].v) == dbits(0.4));
  REQUIRE(jit_out[4].t == 5); REQUIRE(jit_out[4].port_id == 0); REQUIRE(dbits(jit_out[4].v) == dbits(2.0));

  // FE parity.
  auto fe_out = run_fe(kJson, inputs);
  require_demux_parity(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Demultiplexer multi-emit case: when multiple controls are true on the same
// tick the FE emits to all matching output ports. JIT must produce the same
// number of records with the same (port_id, time, value).
//
// Setup: data passes through. c1 = (v > 0), c2 = (v >= 0), c3 = (v >= 1).
// For v=2, all three controls are true -> emit on ports 0, 1, 2.
// For v=0.5, c1+c2 are true -> emit on ports 0, 1.
// For v=-1, no control is true -> no emit.
// ---------------------------------------------------------------------------
SCENARIO("JIT Demultiplexer multi-emit fires multiple ports per tick",
         "[demux][parity][multiemit]") {
  const char* kJson = R"({
    "title": "Demux multi-emit",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1", "o2", "o3"] },
    "operators": [
      { "id": "in",   "type": "Input", "portTypes": ["number"] },
      { "id": "data", "type": "Identity" },
      { "id": "c1",   "type": "CompareGT",  "value": 0.0 },
      { "id": "c2",   "type": "CompareGTE", "value": 0.0 },
      { "id": "c3",   "type": "CompareGTE", "value": 1.0 },
      { "id": "dmx",  "type": "Demultiplexer", "numPorts": 3 },
      { "id": "out",  "type": "Output", "portTypes": ["number", "number", "number"] }
    ],
    "connections": [
      { "from": "in",   "to": "data", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "c1",   "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "c2",   "fromPort": "o1", "toPort": "i1" },
      { "from": "in",   "to": "c3",   "fromPort": "o1", "toPort": "i1" },
      { "from": "data", "to": "dmx",  "fromPort": "o1", "toPort": "i1" },
      { "from": "c1",   "to": "dmx",  "fromPort": "o1", "toPort": "c1" },
      { "from": "c2",   "to": "dmx",  "fromPort": "o1", "toPort": "c2" },
      { "from": "c3",   "to": "dmx",  "fromPort": "o1", "toPort": "c3" },
      { "from": "dmx",  "to": "out",  "fromPort": "o1", "toPort": "i1" },
      { "from": "dmx",  "to": "out",  "fromPort": "o2", "toPort": "i2" },
      { "from": "dmx",  "to": "out",  "fromPort": "o3", "toPort": "i3" }
    ]
  })";

  std::vector<Sample> inputs = {
    {1,  2.0},   // c1, c2, c3 all true -> 3 records
    {2,  0.5},   // c1, c2 true -> 2 records
    {3, -1.0},   // none true -> 0 records
    {4,  0.0},   // c2 true (>= 0) -> 1 record
    {5,  1.0},   // all true -> 3 records
  };

  auto jit_out = run_jit_demux(kJson, inputs);

  // Build expected (sorted by t, then port_id).
  struct Exp { std::int64_t t; std::int32_t pid; double v; };
  std::vector<Exp> expected = {
    {1, 0, 2.0}, {1, 1, 2.0}, {1, 2, 2.0},
    {2, 0, 0.5}, {2, 1, 0.5},
    {4, 1, 0.0},
    {5, 0, 1.0}, {5, 1, 1.0}, {5, 2, 1.0},
  };

  REQUIRE(jit_out.size() == expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    INFO("emit index " << i << " jit=(t=" << jit_out[i].t
         << ", pid=" << jit_out[i].port_id << ", v=" << jit_out[i].v << ")");
    REQUIRE(jit_out[i].t == expected[i].t);
    REQUIRE(jit_out[i].port_id == expected[i].pid);
    REQUIRE(dbits(jit_out[i].v) == dbits(expected[i].v));
  }

  // FE parity.
  auto fe_out = run_fe(kJson, inputs);
  require_demux_parity(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Multiplexer (numPorts=2). Selects one of two upstream streams based on a
// boolean control. JIT graph:
//   data0 := v + 100       (AddScalar 100.0)
//   data1 := v - 100       (AddScalar -100.0)
//   c0    := (v >= 0)      -> select data0 when v >= 0
//   c1    := (v < 0)       -> select data1 when v < 0
// At each tick exactly one control is true; output = data{i} where ctrl[i] is true.
// ---------------------------------------------------------------------------
SCENARIO("JIT Multiplexer N=2 forwards selected data", "[mux][parity]") {
  const char* kJson = R"({
    "title": "Mux N=2",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",    "type": "Input", "portTypes": ["number"] },
      { "id": "data0", "type": "Add",   "value":  100.0 },
      { "id": "data1", "type": "Add",   "value": -100.0 },
      { "id": "c0",    "type": "CompareGTE", "value": 0.0 },
      { "id": "c1",    "type": "CompareLT",  "value": 0.0 },
      { "id": "mux",   "type": "Multiplexer", "numPorts": 2 },
      { "id": "out",   "type": "Output", "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in",    "to": "data0", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",    "to": "data1", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",    "to": "c0",    "fromPort": "o1", "toPort": "i1" },
      { "from": "in",    "to": "c1",    "fromPort": "o1", "toPort": "i1" },
      { "from": "data0", "to": "mux",   "fromPort": "o1", "toPort": "i1" },
      { "from": "data1", "to": "mux",   "fromPort": "o1", "toPort": "i2" },
      { "from": "c0",    "to": "mux",   "fromPort": "o1", "toPort": "c1" },
      { "from": "c1",    "to": "mux",   "fromPort": "o1", "toPort": "c2" },
      { "from": "mux",   "to": "out",   "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  std::vector<Sample> inputs = {
    {1,  5.0},  // c0=true -> data0 = 105.0
    {2, -3.0}, // c1=true -> data1 = -103.0
    {3,  0.0},  // c0=true -> data0 = 100.0
    {4, -1.5}, // c1=true -> data1 = -101.5
  };

  auto jit_out = run_jit_demux(kJson, inputs);

  REQUIRE(jit_out.size() == 4);
  REQUIRE(jit_out[0].t == 1); REQUIRE(dbits(jit_out[0].v) == dbits(105.0));
  REQUIRE(jit_out[1].t == 2); REQUIRE(dbits(jit_out[1].v) == dbits(-103.0));
  REQUIRE(jit_out[2].t == 3); REQUIRE(dbits(jit_out[2].v) == dbits(100.0));
  REQUIRE(jit_out[3].t == 4); REQUIRE(dbits(jit_out[3].v) == dbits(-101.5));

  // FE parity.
  auto fe_out = run_fe(kJson, inputs);
  require_demux_parity(jit_out, fe_out);
}

// ---------------------------------------------------------------------------
// Multiplexer (numPorts=3). Pick one of three streams based on band membership.
// Bands: v in [.., 0), [0, 1), [1, ..). Controls c0/c1/c2 are mutually
// exclusive predicates.
//   data0 := v * 10
//   data1 := v * 100
//   data2 := v * 1000
// ---------------------------------------------------------------------------
SCENARIO("JIT Multiplexer N=3 forwards selected data", "[mux][parity]") {
  const char* kJson = R"({
    "title": "Mux N=3",
    "apiVersion": "v1",
    "entryOperator": "in",
    "output": { "out": ["o1"] },
    "operators": [
      { "id": "in",    "type": "Input", "portTypes": ["number"] },
      { "id": "data0", "type": "Scale", "value": 10.0 },
      { "id": "data1", "type": "Scale", "value": 100.0 },
      { "id": "data2", "type": "Scale", "value": 1000.0 },
      { "id": "c0",    "type": "CompareLT",  "value": 0.0 },
      { "id": "c1a",   "type": "CompareGTE", "value": 0.0 },
      { "id": "c1b",   "type": "CompareLT",  "value": 1.0 },
      { "id": "c1",    "type": "LogicalAnd" },
      { "id": "c2",    "type": "CompareGTE", "value": 1.0 },
      { "id": "mux",   "type": "Multiplexer", "numPorts": 3 },
      { "id": "out",   "type": "Output", "portTypes": ["number"] }
    ],
    "connections": [
      { "from": "in",    "to": "data0", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",    "to": "data1", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",    "to": "data2", "fromPort": "o1", "toPort": "i1" },
      { "from": "in",    "to": "c0",    "fromPort": "o1", "toPort": "i1" },
      { "from": "in",    "to": "c1a",   "fromPort": "o1", "toPort": "i1" },
      { "from": "in",    "to": "c1b",   "fromPort": "o1", "toPort": "i1" },
      { "from": "in",    "to": "c2",    "fromPort": "o1", "toPort": "i1" },
      { "from": "c1a",   "to": "c1",    "fromPort": "o1", "toPort": "i1" },
      { "from": "c1b",   "to": "c1",    "fromPort": "o1", "toPort": "i2" },
      { "from": "data0", "to": "mux",   "fromPort": "o1", "toPort": "i1" },
      { "from": "data1", "to": "mux",   "fromPort": "o1", "toPort": "i2" },
      { "from": "data2", "to": "mux",   "fromPort": "o1", "toPort": "i3" },
      { "from": "c0",    "to": "mux",   "fromPort": "o1", "toPort": "c1" },
      { "from": "c1",    "to": "mux",   "fromPort": "o1", "toPort": "c2" },
      { "from": "c2",    "to": "mux",   "fromPort": "o1", "toPort": "c3" },
      { "from": "mux",   "to": "out",   "fromPort": "o1", "toPort": "i1" }
    ]
  })";

  std::vector<Sample> inputs = {
    {1, -2.0},  // c0 -> data0 = -20.0
    {2,  0.5},  // c1 -> data1 = 50.0
    {3,  3.0},  // c2 -> data2 = 3000.0
    {4,  0.0},  // c1 -> data1 = 0.0
    {5,  1.0},  // c2 -> data2 = 1000.0
  };

  auto jit_out = run_jit_demux(kJson, inputs);

  REQUIRE(jit_out.size() == 5);
  REQUIRE(jit_out[0].t == 1); REQUIRE(dbits(jit_out[0].v) == dbits(-20.0));
  REQUIRE(jit_out[1].t == 2); REQUIRE(dbits(jit_out[1].v) == dbits(50.0));
  REQUIRE(jit_out[2].t == 3); REQUIRE(dbits(jit_out[2].v) == dbits(3000.0));
  REQUIRE(jit_out[3].t == 4); REQUIRE(dbits(jit_out[3].v) == dbits(0.0));
  REQUIRE(jit_out[4].t == 5); REQUIRE(dbits(jit_out[4].v) == dbits(1000.0));

  // FE parity.
  auto fe_out = run_fe(kJson, inputs);
  require_demux_parity(jit_out, fe_out);
}
