#include <algorithm>

#include <catch2/catch.hpp>

#include "rtbot/compiled/jit/CompiledGraph.h"
#include "rtbot/compiled/jit/JsonParser.h"
#include "rtbot/compiled/jit/SegmentPartitioner.h"

using namespace rtbot::jit;

namespace {

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

SCENARIO("Bollinger graph partitions into one segment", "[partitioner]") {
  CompiledGraph g = parse_program_json(kBollingerJson);
  PartitionResult result = partition_segments(g);

  REQUIRE(result.segments.size() == 1);
  REQUIRE(result.sync_ops.empty());
  REQUIRE(result.segments[0].op_ids.size() == 8);

  // Check topological correctness: Input ("754") is first.
  REQUIRE(result.segments[0].op_ids.front() == "754");

  // Output ("37") is last.
  REQUIRE(result.segments[0].op_ids.back() == "37");

  auto pos = [&](const std::string& id) {
    auto& v = result.segments[0].op_ids;
    return std::find(v.begin(), v.end(), id) - v.begin();
  };

  REQUIRE(pos("262") < pos("510"));   // Resampler before MA
  REQUIRE(pos("262") < pos("865"));   // Resampler before StdDev
  REQUIRE(pos("510") < pos("861"));   // MA before Add
  REQUIRE(pos("510") < pos("495"));   // MA before Sub
  REQUIRE(pos("865") < pos("996"));   // StdDev before Scale
  REQUIRE(pos("996") < pos("861"));   // Scale before Add
  REQUIRE(pos("996") < pos("495"));   // Scale before Sub
}

SCENARIO("Graph with Join op produces multiple segments", "[partitioner]") {
  CompiledGraph g;
  g.entry_op_id = "in";
  g.nodes.push_back({"in",   OpKind::Input,         0, 0.0, 0, {}});
  g.nodes.push_back({"ma1",  OpKind::MovingAverage,  5, 0.0, 0, {}});
  g.nodes.push_back({"ma2",  OpKind::MovingAverage, 30, 0.0, 0, {}});
  g.nodes.push_back({"join", OpKind::Join,           0, 0.0, 0, {}});
  g.nodes.push_back({"sub",  OpKind::Sub,            0, 0.0, 0, {}});
  g.nodes.push_back({"out",  OpKind::Output,         0, 0.0, 0, {}});
  g.connections.push_back({"in",   0, "ma1",  0});
  g.connections.push_back({"in",   0, "ma2",  0});
  g.connections.push_back({"ma1",  0, "join", 0});
  g.connections.push_back({"ma2",  0, "join", 1});
  g.connections.push_back({"join", 0, "sub",  0});
  g.connections.push_back({"sub",  0, "out",  0});

  PartitionResult result = partition_segments(g);

  REQUIRE(result.sync_ops.size() == 1);
  REQUIRE(result.sync_ops[0] == "join");
  // Pre-join segment (in, ma1, ma2) and post-join segment (sub, out).
  REQUIRE(result.segments.size() >= 2);
}

SCENARIO("Trivial graph partitions cleanly", "[partitioner]") {
  CompiledGraph g;
  g.entry_op_id = "in";
  g.nodes.push_back({"in",  OpKind::Input,  0, 0.0, 0, {}});
  g.nodes.push_back({"out", OpKind::Output, 0, 0.0, 0, {}});
  g.connections.push_back({"in", 0, "out", 0});

  PartitionResult result = partition_segments(g);

  REQUIRE(result.segments.size() == 1);
  REQUIRE(result.sync_ops.empty());
  REQUIRE(result.segments[0].op_ids.size() == 2);
}
