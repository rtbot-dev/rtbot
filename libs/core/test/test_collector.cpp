#include <catch2/catch.hpp>

#include <memory>
#include <vector>

#include "rtbot/Collector.h"
#include "rtbot/Message.h"
#include "rtbot/PortType.h"

using namespace rtbot;

SCENARIO("Collector requires at least one port type", "[collector]") {
  REQUIRE_THROWS_AS(Collector("c_empty", {}), std::runtime_error);
}

SCENARIO("Collector rejects invalid port type names", "[collector]") {
  REQUIRE_THROWS_AS(
      Collector("c_bad", std::vector<std::string>{"not_a_port_type"}),
      std::runtime_error);
}

SCENARIO("Collector rejects out-of-range data port indices", "[collector]") {
  Collector c("c1", std::vector<std::string>{PortType::NUMBER, PortType::BOOLEAN});
  REQUIRE(c.num_data_ports() == 2);

  // Valid ports accept without throwing.
  REQUIRE_NOTHROW(c.receive_data(
      create_message<NumberData>(1, NumberData{1.0}), 0));
  REQUIRE_NOTHROW(c.receive_data(
      create_message<BooleanData>(2, BooleanData{true}), 1));

  // Out-of-range port index on a valid Collector throws, not crashes.
  REQUIRE_THROWS_AS(
      c.receive_data(create_message<NumberData>(3, NumberData{2.0}), 2),
      std::runtime_error);
  REQUIRE_THROWS_AS(
      c.receive_data(create_message<NumberData>(4, NumberData{3.0}), 99),
      std::runtime_error);
}

SCENARIO("Collector rejects out-of-range control port indices", "[collector]") {
  // Collector has no control ports by default; any index should throw.
  Collector c("c2", std::vector<std::string>{PortType::NUMBER});
  REQUIRE(c.num_control_ports() == 0);
  REQUIRE_THROWS_AS(
      c.receive_control(create_message<NumberData>(1, NumberData{1.0}), 0),
      std::runtime_error);
}
