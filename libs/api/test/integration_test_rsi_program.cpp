#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include <nlohmann/json.hpp>

#include "rtbot/Program.h"

using namespace rtbot;

// Helper function to create RSI program JSON with configurable period
std::string create_rsi_program(size_t n) {
  double alpha = 1.0 / static_cast<double>(n);
  double beta = (n - 1.0) / static_cast<double>(n);

  return R"({
   "operators": [
     {"id": "input", "type": "Input", "portTypes": ["number"]},
     {"id": "output", "type": "Output", "portTypes": ["number"]},
     {"id": "count", "type": "CountNumber"},
     {"id": "dm", "type": "Demultiplexer", "numPorts": 2},
     {"id": "lt", "type": "LessThan", "value": )" +
         std::to_string(n + 1.0) + R"(},
     {"id": "et", "type": "EqualTo", "value": )" +
         std::to_string(n + 1.0) + R"(},
     {"id": "gt", "type": "GreaterThan", "value": )" +
         std::to_string(n + 1.0) + R"(},
     {"id": "etn2", "type": "EqualTo", "value": )" +
         std::to_string(n + 2.0) + R"(},
     {"id": "diff1", "type": "Difference"},
     {"id": "diff2", "type": "Difference"},
     {"id": "gt0", "type": "GreaterThan", "value": 0.0},
     {"id": "et0", "type": "EqualTo", "value": 0.0},
     {"id": "lt0", "type": "LessThan", "value": 0.0},
     {"id": "sum0", "type": "CumulativeSum"},
     {"id": "sc0", "type": "Scale", "value": )" +
         std::to_string(alpha) + R"(},
     {"id": "l1", "type": "Linear", "coefficients": [)" +
         std::to_string(beta) + "," + std::to_string(alpha) + R"(]},
     {"id": "l2", "type": "Linear", "coefficients": [)" +
         std::to_string(beta) + "," + std::to_string(alpha) + R"(]},
     {"id": "neg0", "type": "Scale", "value": -1.0},
     {"id": "sum1", "type": "CumulativeSum"},
     {"id": "sc1", "type": "Scale", "value": )" +
         std::to_string(alpha) + R"(},
     {"id": "gt1", "type": "GreaterThan", "value": 0.0},
     {"id": "et1", "type": "EqualTo", "value": 0.0},
     {"id": "lt1", "type": "LessThan", "value": 0.0},
     {"id": "const0", "type": "ConstantNumber", "value": 0.0},
     {"id": "const1", "type": "ConstantNumber", "value": 0.0},
     {"id": "neg1", "type": "Scale", "value": -1.0},
     {"id": "varg", "type": "Variable", "default_value": 0.0},
     {"id": "varl", "type": "Variable", "default_value": 0.0},
     {"id": "ts1", "type": "TimeShift", "shift": 1},
     {"id": "ts2", "type": "TimeShift", "shift": 1},
     {"id": "divide", "type": "Division"},
     {"id": "add1", "type": "Add", "value": 1.0},
     {"id": "power_1", "type": "Power", "value": -1.0},
     {"id": "scale_100", "type": "Scale", "value": -100.0},
     {"id": "add100", "type": "Add", "value": 100.0},
     {"id": "cgtz", "type": "ConstantNumberToBoolean", "value": false},
     {"id": "cgto", "type": "ConstantNumberToBoolean", "value": true},
     {"id": "cltz", "type": "ConstantNumberToBoolean", "value": false},
     {"id": "clto", "type": "ConstantNumberToBoolean", "value": true},
     {"id": "ceto", "type": "ConstantNumberToBoolean", "value": true}
   ],
   "connections": [
     {"from": "input", "to": "dm", "fromPort": "o1", "toPort": "i1"},
     {"from": "input", "to": "count"},
     {"from": "count", "to": "lt"},
     {"from": "count", "to": "gt"},
     {"from": "count", "to": "et"},
     {"from": "count", "to": "etn2"},
     {"from": "lt", "to": "clto"},
     {"from": "clto", "to": "dm", "toPort": "c1"},
     {"from": "lt", "to": "cltz"},
     {"from": "cltz", "to": "dm", "toPort": "c2"},
     {"from": "et", "to": "ceto"},
     {"from": "ceto", "to": "dm", "toPort": "c1"},
     {"from": "ceto", "to": "dm", "toPort": "c2"},
     {"from": "gt", "to": "cgto"},
     {"from": "gt", "to": "cgtz"},
     {"from": "cgto", "to": "dm", "toPort": "c2"},
     {"from": "cgtz", "to": "dm", "toPort": "c1"},
     {"from": "dm", "to": "diff1", "fromPort": "o1", "toPort": "i1"},
     {"from": "diff1", "to": "gt0"},
     {"from": "gt0", "to": "sum0"},
     {"from": "sum0", "to": "sc0"},
     {"from": "sc0", "to": "varg"},
     {"from": "varg", "to": "ts1"},
     {"from": "ts1", "to": "l1", "toPort": "i1"},
     {"from": "l1", "to": "ts1"},
     {"from": "diff1", "to": "et0"},
     {"from": "et0", "to": "sum0"},
     {"from": "et0", "to": "sum1"},
     {"from": "diff1", "to": "lt0"},
     {"from": "lt0", "to": "neg0"},
     {"from": "neg0", "to": "sum1"},
     {"from": "sum1", "to": "sc1"},
     {"from": "sc1", "to": "varl"},
     {"from": "varl", "to": "ts2"},
     {"from": "ts2", "to": "l2", "toPort": "i1"},
     {"from": "l2", "to": "ts2"},
     {"from": "dm", "to": "diff2", "fromPort": "o2", "toPort": "i1"},
     {"from": "diff2", "to": "gt1"},
     {"from": "gt1", "to": "l1", "toPort": "i2"},
     {"from": "gt1", "to": "const0"},
     {"from": "const0", "to": "l2", "toPort": "i2"},
     {"from": "diff2", "to": "lt1"},
     {"from": "lt1", "to": "neg1"},
     {"from": "neg1", "to": "l2", "toPort": "i2"},
     {"from": "lt1", "to": "const1"},
     {"from": "const1", "to": "l1", "toPort": "i2"},
     {"from": "diff2", "to": "et1"},
     {"from": "et1", "to": "l1", "toPort": "i2"},
     {"from": "et1", "to": "l2", "toPort": "i2"},
     {"from": "etn2", "to": "varg"},
     {"from": "et", "to": "varg", "toPort": "c1"},
     {"from": "etn2", "to": "varl"},
     {"from": "et", "to": "varl", "toPort": "c1"},
     {"from": "varg", "to": "divide", "toPort": "i1"},
     {"from": "varl", "to": "divide", "toPort": "i2"},
     {"from": "l1", "to": "divide", "toPort": "i1"},
     {"from": "l2", "to": "divide", "toPort": "i2"},
     {"from": "divide", "to": "add1"},
     {"from": "add1", "to": "power_1"},
     {"from": "power_1", "to": "scale_100"},
     {"from": "scale_100", "to": "add100"},
     {"from": "add100", "to": "output"}
   ],
   "entryOperator": "input",
   "output": {
     "output": ["o1"]
   }
 })";
}

SCENARIO("RSI calculation using Program JSON configuration", "[rsi][program]") {
  GIVEN("A Program initialized with RSI calculation JSON") {
    const size_t n = 14;  // RSI period
    auto json_program = create_rsi_program(n);
    Program program(json_program);

    std::cout << "Program created" << std::endl;

    WHEN("Processing a sequence of price data") {
      std::vector<std::pair<timestamp_t, double>> test_data = {
          {1, 54.8},   {2, 56.8},   {3, 57.85},  {4, 59.85},  {5, 60.57},  {6, 61.1},   {7, 62.17},  {8, 60.6},
          {9, 62.35},  {10, 62.15}, {11, 62.35}, {12, 61.45}, {13, 62.8},  {14, 61.37}, {15, 62.5},  {16, 62.57},
          {17, 60.8},  {18, 59.37}, {19, 60.35}, {20, 62.35}, {21, 62.17}, {22, 62.55}, {23, 64.55}, {24, 64.37},
          {25, 65.3},  {26, 64.42}, {27, 62.9},  {28, 61.6},  {29, 62.05}, {30, 60.05}, {31, 59.7},  {32, 60.9},
          {33, 60.25}, {34, 58.27}, {35, 58.7},  {36, 57.72}, {37, 58.1},  {38, 58.2}};

      std::vector<double> expected_values = {74.21384, 74.33552, 65.87129, 59.93370, 62.43288, 66.96205,
                                             66.18862, 67.05377, 71.22679, 70.36299, 72.23644, 67.86486,
                                             60.99822, 55.79821, 57.15964, 49.81579, 48.63810, 52.76154,
                                             50.40119, 43.95111, 45.57992, 42.54534, 44.09946, 44.52472};

      std::vector<std::pair<timestamp_t, double>> outputs;

      for (const auto& [time, price] : test_data) {
        auto batch = program.receive(Message<NumberData>(time, NumberData{price}));

        if (!batch.empty() && batch.count("output") > 0 && !batch["output"]["o1"].empty()) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(batch["output"]["o1"][0].get());
          outputs.emplace_back(msg->time, msg->data.value);
        }
      }

      THEN("Output matches expected RSI behavior") {
        REQUIRE(outputs.size() == expected_values.size());
        for (size_t i = 0; i < outputs.size(); ++i) {
          REQUIRE(outputs[i].first == n + i + 1);  // 15, 16, 17, ...
          REQUIRE(outputs[i].second == Approx(expected_values[i]).margin(0.00001));
        }
      }
    }
  }
}