#include <catch2/catch.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

#include "rtbot/Program.h"
#include "rtbot/bindings.h"

using namespace rtbot;
using json = nlohmann::json;

TEST_CASE("Basic Flow Pipeline Test", "[basic_pipeline]") {
  SECTION("Test Flow Pipeline") {
    // Load program JSON
    json program_json;
    {
      std::ifstream in("examples/data/program-test-5.json");
      REQUIRE(in.good());
      in >> program_json;
    }

    // Create program
    auto& manager = ProgramManager::instance();
    manager.clear_all_programs();

    std::string validate_result = validate_program(program_json.dump());
    std::cout << "Validate result: " << pretty_print_validation_error(validate_result) << std::endl;

    std::string create_result = manager.create_program("test_prog", program_json.dump());
    std::cout << "Create result: " << create_result << std::endl;
    REQUIRE(create_result.empty());

    std::vector<uint64_t> times;
    std::vector<double> values;
    std::vector<std::string> ports;

    const uint64_t start = 1;
    const uint64_t end = 20;

    for (uint64_t i = start; i <= end; i++) {
      times.push_back(i);
      values.push_back(i * i);
      ports.push_back("i1");
    }

    // Process data in batches and verify outputs
    std::string returned = process_batch("test_prog", times, values, ports);
    auto result = json::parse(returned);

    if (result.contains("output")) {
      const auto& op_outputs = result["output"];

      if (op_outputs.contains("o1")) {
        int time = 11;
        for (const auto& msg : op_outputs["o1"]) {
          uint64_t current_time = msg["time"];
          double current_value = msg["value"];

          REQUIRE(current_time == time);
          REQUIRE(current_value == time * time);
          time++;
        }
      } else
        REQUIRE(1 == 0);
    } else
      REQUIRE(1 == 0);
  }
}