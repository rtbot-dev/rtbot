#include <catch2/catch.hpp>
#include <cmath>
#include <nlohmann/json.hpp>

#include "rtbot/Collector.h"
#include "rtbot/Program.h"
#include "rtbot/finance/RelativeStrengthIndex.h"

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
         std::to_string(n + 1) + R"(},
     {"id": "et", "type": "EqualTo", "value": )" +
         std::to_string(n + 1) + R"(},
     {"id": "gt", "type": "GreaterThan", "value": )" +
         std::to_string(n + 1) + R"(},
     {"id": "etn2", "type": "EqualTo", "value": )" +
         std::to_string(n + 2) + R"(},
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
     {"id": "ts11", "type": "TimeShift", "shift": 1},
     {"id": "ts2", "type": "TimeShift", "shift": 1},
     {"id": "ts22", "type": "TimeShift", "shift": 1},
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
     {"from": "l1", "to": "ts11"},
     {"from": "ts11", "to": "l1"},
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
     {"from": "l2", "to": "ts22"},
     {"from": "ts22", "to": "l2"},
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
     {"from": "etn2", "to": "varl"},
     {"from": "et", "to": "varg", "toPort": "c1"},
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
          for (const auto& msg_ptr : batch["output"]["o1"]) {
            const auto* msg = dynamic_cast<const Message<NumberData>*>(msg_ptr.get());
            outputs.emplace_back(msg->time, msg->data.value);
          }
        }
      }

      THEN("Output matches expected RSI behavior") {
        // REQUIRE(outputs.size() == expected_values.size());
        for (size_t i = 0; i < outputs.size(); ++i) {
          // std::cout << outputs[i].first << ", " << outputs[i].second << std::endl;
          REQUIRE(outputs[i].first == n + i + 1);  // 15, 16, 17, ...
          REQUIRE(outputs[i].second == Approx(expected_values[i]).margin(0.00001));
        }
      }
    }
  }
}

SCENARIO("RSI program maintains state through serialization with large dataset", "[rsi][program][serialization]") {
  GIVEN("A Program initialized with RSI calculation JSON") {
    const size_t n = 14;  // RSI period
    auto json_program = create_rsi_program(n);
    Program original_program(json_program);

    // 100 price points simulating realistic market data
    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 44.34},  {2, 44.09},  {3, 44.15},  {4, 43.61},  {5, 44.33},  {6, 44.83},  {7, 45.10},  {8, 45.15},
        {9, 43.61},  {10, 44.33}, {11, 44.83}, {12, 45.10}, {13, 45.15}, {14, 46.92}, {15, 46.75}, {16, 46.25},
        {17, 45.71}, {18, 46.45}, {19, 45.78}, {20, 45.35}, {21, 44.03}, {22, 44.18}, {23, 44.22}, {24, 44.57},
        {25, 43.42}, {26, 42.66}, {27, 43.13}, {28, 43.82}, {29, 44.83}, {30, 45.00}, {31, 45.61}, {32, 46.28},
        {33, 46.00}, {34, 45.61}, {35, 44.33}, {36, 43.61}, {37, 44.83}, {38, 45.10}, {39, 46.75}, {40, 46.25},
        {41, 45.71}, {42, 46.45}, {43, 45.78}, {44, 46.00}, {45, 46.41}, {46, 46.22}, {47, 45.64}, {48, 46.21},
        {49, 46.25}, {50, 45.71}, {51, 46.45}, {52, 45.78}, {53, 45.35}, {54, 44.03}, {55, 44.18}, {56, 44.22},
        {57, 44.57}, {58, 43.42}, {59, 42.66}, {60, 43.13}, {61, 43.82}, {62, 44.83}, {63, 45.00}, {64, 45.61},
        {65, 46.28}, {66, 46.00}, {67, 45.61}, {68, 44.33}, {69, 43.61}, {70, 44.83}, {71, 45.10}, {72, 46.75},
        {73, 46.25}, {74, 45.71}, {75, 46.45}, {76, 45.78}, {77, 46.00}, {78, 46.41}, {79, 46.22}, {80, 45.64},
        {81, 46.21}, {82, 46.25}, {83, 45.71}, {84, 46.45}, {85, 45.78}, {86, 45.35}, {87, 44.03}, {88, 44.18},
        {89, 44.22}, {90, 44.57}, {91, 43.42}, {92, 42.66}, {93, 43.13}, {94, 43.82}, {95, 44.83}, {96, 45.00},
        {97, 45.61}, {98, 46.28}, {99, 46.00}, {100, 45.61}};

    WHEN("Processing 70 points, serializing, then processing remaining 30 points") {
      const size_t split_idx = 70;

      for (size_t i = 0; i < split_idx; i++) {
        original_program.receive(Message<NumberData>(inputs[i].first, NumberData{inputs[i].second}));
      }

      auto serialized = original_program.serialize_data();
      Program restored_program(json_program);
      restored_program.restore_data_from_json(serialized);

      std::vector<std::pair<timestamp_t, double>> outputs_original;
      std::vector<std::pair<timestamp_t, double>> outputs_restored;

      for (size_t i = split_idx; i < inputs.size(); i++) {
        auto batch_orig = original_program.receive(Message<NumberData>(inputs[i].first, NumberData{inputs[i].second}));
        auto batch_rest = restored_program.receive(Message<NumberData>(inputs[i].first, NumberData{inputs[i].second}));

        if (!batch_orig.empty() && batch_orig.count("output") > 0 && !batch_orig["output"]["o1"].empty()) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(batch_orig["output"]["o1"][0].get());
          outputs_original.emplace_back(msg->time, msg->data.value);
        }

        if (!batch_rest.empty() && batch_rest.count("output") > 0 && !batch_rest["output"]["o1"].empty()) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(batch_rest["output"]["o1"][0].get());
          outputs_restored.emplace_back(msg->time, msg->data.value);
        }
      }

      THEN("Both programs produce identical outputs with matching timestamps") {
        REQUIRE(outputs_original.size() == outputs_restored.size());
        REQUIRE(!outputs_original.empty());
        for (size_t i = 0; i < outputs_original.size(); ++i) {
          REQUIRE(outputs_original[i].first == outputs_restored[i].first);
          REQUIRE(outputs_original[i].second == Approx(outputs_restored[i].second));
        }
      }
    }
  }
}

SCENARIO("RSI JSON program produces same output as RelativeStrengthIndex operator", "[rsi][program][finance]") {
  GIVEN("A JSON RSI program and a RelativeStrengthIndex operator with the same period") {
    const size_t n = 14;
    auto json_program = create_rsi_program(n);
    Program program(json_program);
    auto rsi = std::make_shared<RelativeStrengthIndex>("rsi", n);
    auto rsi_col = std::make_shared<Collector>("rc", std::vector<std::string>{"number"});
    rsi->connect(rsi_col, 0, 0);

    std::vector<std::pair<timestamp_t, double>> test_data = {
        {1, 54.8},   {2, 56.8},   {3, 57.85},  {4, 59.85},  {5, 60.57},  {6, 61.1},   {7, 62.17},  {8, 60.6},
        {9, 62.35},  {10, 62.15}, {11, 62.35}, {12, 61.45}, {13, 62.8},  {14, 61.37}, {15, 62.5},  {16, 62.57},
        {17, 60.8},  {18, 59.37}, {19, 60.35}, {20, 62.35}, {21, 62.17}, {22, 62.55}, {23, 64.55}, {24, 64.37},
        {25, 65.3},  {26, 64.42}, {27, 62.9},  {28, 61.6},  {29, 62.05}, {30, 60.05}, {31, 59.7},  {32, 60.9},
        {33, 60.25}, {34, 58.27}, {35, 58.7},  {36, 57.72}, {37, 58.1},  {38, 58.2}};

    WHEN("Processing the same data through both") {
      std::map<timestamp_t, double> program_outputs;
      std::map<timestamp_t, double> operator_outputs;

      for (const auto& [time, price] : test_data) {
        // Process through JSON program
        auto batch = program.receive(Message<NumberData>(time, NumberData{price}));
        if (!batch.empty() && batch.count("output") > 0 && !batch["output"]["o1"].empty()) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(batch["output"]["o1"][0].get());
          program_outputs[msg->time] = msg->data.value;
        }

        // Process through finance operator
        rsi->receive_data(create_message<NumberData>(time, NumberData{price}), 0);
        rsi->execute();
        const auto& output = rsi_col->get_data_queue(0);
        if (!output.empty()) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(output.front().get());
          operator_outputs[msg->time] = msg->data.value;
        }
        rsi_col->reset();
      }

      THEN("Both produce the same RSI values for overlapping timestamps") {
        REQUIRE(!program_outputs.empty());
        REQUIRE(!operator_outputs.empty());

        // Compare values at timestamps that both produced output for
        size_t matched = 0;
        for (const auto& [time, prog_value] : program_outputs) {
          auto it = operator_outputs.find(time);
          if (it != operator_outputs.end()) {
            REQUIRE(prog_value == Approx(it->second).margin(0.00001));
            matched++;
          }
        }
        REQUIRE(matched == program_outputs.size());
      }
    }
  }
}

SCENARIO("RSI program maintains state through serialization", "[rsi][program][serialization]") {
  GIVEN("A Program initialized with RSI calculation JSON") {
    const size_t n = 14;  // RSI period
    auto json_program = create_rsi_program(n);
    Program original_program(json_program);

    std::vector<std::pair<timestamp_t, double>> inputs = {
        {1, 54.8},  {2, 56.8},   {3, 57.85},  {4, 59.85},  {5, 60.57},  {6, 61.1},   {7, 62.17},  {8, 60.6},
        {9, 62.35}, {10, 62.15}, {11, 62.35}, {12, 61.45}, {13, 62.8},  {14, 61.37}, {15, 62.5},  {16, 62.57},
        {17, 60.8}, {18, 59.37}, {19, 60.35}, {20, 62.35}, {21, 62.17}, {22, 62.55}, {23, 64.55}, {24, 64.37}};

    WHEN("Processing partial data, serializing, then processing remaining data") {
      // Pick a random split point after initial period
      const size_t split_idx = n + 5;  // Process first 19 points

      // Process initial data with original program
      for (size_t i = 0; i < split_idx; i++) {
        original_program.receive(Message<NumberData>(inputs[i].first, NumberData{inputs[i].second}));
      }

      // Serialize and restore program
      auto serialized = original_program.serialize_data();
      Program restored_program(json_program);
      restored_program.restore_data_from_json(serialized);

      // Process remaining data with both programs
      std::vector<double> outputs_original;
      std::vector<double> outputs_restored;

      for (size_t i = split_idx; i < inputs.size(); i++) {
        auto batch_orig = original_program.receive(Message<NumberData>(inputs[i].first, NumberData{inputs[i].second}));
        auto batch_rest = restored_program.receive(Message<NumberData>(inputs[i].first, NumberData{inputs[i].second}));

        if (!batch_orig.empty() && batch_orig.count("output") > 0 && !batch_orig["output"]["o1"].empty()) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(batch_orig["output"]["o1"][0].get());
          outputs_original.push_back(msg->data.value);
        }

        if (!batch_rest.empty() && batch_rest.count("output") > 0 && !batch_rest["output"]["o1"].empty()) {
          const auto* msg = dynamic_cast<const Message<NumberData>*>(batch_rest["output"]["o1"][0].get());
          outputs_restored.push_back(msg->data.value);
        }
      }

      THEN("Both programs produce identical outputs") {
        REQUIRE(outputs_original.size() == outputs_restored.size());
        for (size_t i = 0; i < outputs_original.size(); ++i) {
          REQUIRE(outputs_original[i] == Approx(outputs_restored[i]));
        }
      }
    }
  }
}