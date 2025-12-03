#include <catch2/catch.hpp>

#include "rtbot/OperatorJson.h"

using namespace rtbot;

TEST_CASE("OperatorJson creates PctChange operator", "[OperatorJson][PctChange]") {
  const std::string json = R"({
    "type": "PctChange",
    "id": "pct_op"
  })";

  auto op = OperatorJson::read_op(json);
  REQUIRE(op->type_name() == "PctChange");
  REQUIRE(op->id() == "pct_op");

  const auto serialized = OperatorJson::write_op(op);
  auto round_trip = OperatorJson::read_op(serialized);
  REQUIRE(round_trip->type_name() == "PctChange");
  REQUIRE(round_trip->id() == "pct_op");
}
