#include <set>

#include <catch2/catch.hpp>

#include "rtbot/compiled/jit/JsonParser.h"
#include "rtbot/compiled/jit/StateLayout.h"

using namespace rtbot::jit;

namespace {

// Same Bollinger JSON used in test_json_parser.cpp.
const char* kBollingerJson = R"({
  "title": "Bollinger Bands",
  "apiVersion": "v1",
  "entryOperator": "754",
  "output": { "37": ["o2", "o1", "o3"] },
  "operators": [
    { "id": "37", "type": "Output", "portTypes": ["number", "number", "number"] },
    { "id": "495", "type": "Subtraction" },
    { "id": "861", "type": "Addition" },
    { "id": "996", "type": "Scale", "value": 2 },
    { "id": "865", "type": "StandardDeviation", "window_size": 14 },
    { "id": "510", "type": "MovingAverage", "window_size": 14 },
    { "id": "262", "type": "ResamplerHermite", "interval": 1 },
    { "id": "754", "type": "Input", "portTypes": ["number"] }
  ],
  "connections": [
    { "from": "510", "to": "37", "fromPort": "o1", "toPort": "i3" },
    { "from": "495", "to": "37", "fromPort": "o1", "toPort": "i1" },
    { "from": "861", "to": "37", "fromPort": "o1", "toPort": "i2" },
    { "from": "996", "to": "495", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "495", "fromPort": "o1", "toPort": "i1" },
    { "from": "996", "to": "861", "fromPort": "o1", "toPort": "i2" },
    { "from": "510", "to": "861", "fromPort": "o1", "toPort": "i1" },
    { "from": "865", "to": "996", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "865", "fromPort": "o1", "toPort": "i1" },
    { "from": "262", "to": "510", "fromPort": "o1", "toPort": "i1" },
    { "from": "754", "to": "262", "fromPort": "o1", "toPort": "i1" }
  ]
})";

}  // namespace

SCENARIO("plan_state_layout sizes Bollinger correctly", "[state_layout]") {
  CompiledGraph g = parse_program_json(kBollingerJson);
  StateLayout layout = plan_state_layout(g);

  // Bollinger has 3 stateful ops: ResamplerHermite (11), MovingAverage W=14 (17),
  // StandardDeviation W=14 (17). Total: 45 doubles.
  REQUIRE(layout.total_size == 45);

  // Each op id has an entry in offsets. Stateless ops occupy 0 size each.
  REQUIRE(layout.offsets.size() == g.nodes.size());

  // Stateless ops point to the same offset as the next stateful op (since
  // stateless op size is 0). Just check that they exist.
  REQUIRE(layout.offsets.count("754") == 1);   // Input
  REQUIRE(layout.offsets.count("262") == 1);   // ResamplerHermite
  REQUIRE(layout.offsets.count("510") == 1);   // MovingAverage
  REQUIRE(layout.offsets.count("865") == 1);   // StandardDeviation
  REQUIRE(layout.offsets.count("996") == 1);   // Scale
  REQUIRE(layout.offsets.count("861") == 1);   // Addition
  REQUIRE(layout.offsets.count("495") == 1);   // Subtraction
  REQUIRE(layout.offsets.count("37") == 1);    // Output

  // The stateful offsets should sum to 45 in some valid order. The exact
  // offsets depend on the order the parser emits nodes. Don't assert
  // specific offsets for stateful ops; instead assert their relative spacing
  // by checking total_size and that all three stateful ops have distinct
  // offsets.
  std::set<std::size_t> stateful_offsets;
  for (const auto& node : g.nodes) {
    if (node.kind == OpKind::ResamplerHermite ||
        node.kind == OpKind::MovingAverage ||
        node.kind == OpKind::StdDev) {
      stateful_offsets.insert(layout.offsets.at(node.id));
    }
  }
  REQUIRE(stateful_offsets.size() == 3);
}

SCENARIO("state_size_for returns expected sizes", "[state_layout]") {
  OpNode ma; ma.kind = OpKind::MovingAverage; ma.window_size = 14;
  REQUIRE(state_size_for(ma) == 17);

  OpNode sd; sd.kind = OpKind::StdDev; sd.window_size = 30;
  REQUIRE(state_size_for(sd) == 33);

  OpNode rs; rs.kind = OpKind::ResamplerHermite;
  REQUIRE(state_size_for(rs) == 11);

  OpNode pk; pk.kind = OpKind::PeakDetector; pk.window_size = 11;
  REQUIRE(state_size_for(pk) == 23);  // 2*11 + 1

  OpNode add; add.kind = OpKind::Add;
  REQUIRE(state_size_for(add) == 0);
}
