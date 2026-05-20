#include <catch2/catch.hpp>

#include "rtbot/compiled/jit/JsonParser.h"

using namespace rtbot::jit;

namespace {

// Copy of the bollinger_json string from apps/benchmark/src/benchmark.cpp.
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

SCENARIO("parse_program_json parses Bollinger correctly", "[json][parser]") {
  CompiledGraph g = parse_program_json(kBollingerJson);

  REQUIRE(g.entry_op_id == "754");
  REQUIRE(g.nodes.size() == 8);
  REQUIRE(g.connections.size() == 11);

  // Output mapping: 37 → ["o2", "o1", "o3"]
  REQUIRE(g.outputs.count("37") == 1);
  REQUIRE(g.outputs.at("37").size() == 3);
  REQUIRE(g.outputs.at("37")[0] == "o2");

  // Find specific nodes and check their kind + parameters
  auto find_node = [&](const std::string& id) -> const OpNode* {
    for (const auto& n : g.nodes) if (n.id == id) return &n;
    return nullptr;
  };

  REQUIRE(find_node("754")->kind == OpKind::Input);
  REQUIRE(find_node("262")->kind == OpKind::ResamplerHermite);
  REQUIRE(find_node("262")->resampler_interval == 1);
  REQUIRE(find_node("510")->kind == OpKind::MovingAverage);
  REQUIRE(find_node("510")->window_size == 14);
  REQUIRE(find_node("865")->kind == OpKind::StdDev);
  REQUIRE(find_node("865")->window_size == 14);
  REQUIRE(find_node("996")->kind == OpKind::Scale);
  REQUIRE(find_node("996")->scale_constant == 2.0);
  REQUIRE(find_node("861")->kind == OpKind::Add);
  REQUIRE(find_node("495")->kind == OpKind::Sub);
  REQUIRE(find_node("37")->kind == OpKind::Output);

  // Spot-check one connection: 754:o1 → 262:i1
  bool found = false;
  for (const auto& c : g.connections) {
    if (c.from_id == "754" && c.to_id == "262") {
      REQUIRE(c.from_port == 0);  // "o1" → 0
      REQUIRE(c.to_port == 0);    // "i1" → 0
      found = true;
    }
  }
  REQUIRE(found);
}

SCENARIO("parse_program_json throws on unknown operator type", "[json][parser]") {
  const char* bad = R"({
    "entryOperator": "1",
    "operators": [{ "id": "1", "type": "MysteryOp" }],
    "connections": []
  })";
  REQUIRE_THROWS_AS(parse_program_json(bad), std::runtime_error);
}
