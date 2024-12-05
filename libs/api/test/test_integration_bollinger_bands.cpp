#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

#include "rtbot/Program.h"
#include "rtbot/bindings.h"

using namespace rtbot;
using json = nlohmann::json;

TEST_CASE("Bollinger Bands Pipeline Test", "[program]") {
  SECTION("Test Generated Bollinger Bands") {
    // Load program JSON
    json program_json;
    {
      std::ifstream in("examples/data/program-test-6.json");
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

    // Generate test data
    srand(time(nullptr));
    std::vector<uint64_t> times;
    std::vector<double> values;
    std::vector<std::string> ports;

    const uint64_t start = 0;
    const uint64_t end = 10000;

    // Track metrics
    size_t output_count_up = 0;
    size_t output_count_down = 0;
    size_t output_count_ma = 0;
    uint64_t first_up = 0, first_down = 0, first_ma = 0;
    uint64_t last_up = -1, last_down = -1, last_ma = -1;

    // Generate input sequence
    times.push_back(start);
    values.push_back(start * start);
    ports.push_back("input1");

    for (uint64_t i = start + 1; i < end; i++) {
      if (rand() < (RAND_MAX / 2)) {
        times.push_back(i);
        values.push_back(i * i);
        ports.push_back("i1");
      }
    }

    times.push_back(end);
    values.push_back(end * end);
    ports.push_back("i1");

    // Process data in batches and verify outputs
    auto result = json::parse(process_batch_debug("test_prog", times, values, ports));

    if (result.contains("37")) {
      const auto& op_outputs = result["37"];

      if (op_outputs.contains("o1")) {
        for (const auto& msg : op_outputs["o1"]) {
          uint64_t current_time = msg["time"];
          if (last_up != static_cast<uint64_t>(-1)) {
            REQUIRE(current_time == last_up + 1);
          } else {
            first_up = current_time;
          }
          last_up = current_time;
          output_count_up++;
        }
      }

      if (op_outputs.contains("o2")) {
        for (const auto& msg : op_outputs["o2"]) {
          uint64_t current_time = msg["time"];
          if (last_down != static_cast<uint64_t>(-1)) {
            REQUIRE(current_time == last_down + 1);
          } else {
            first_down = current_time;
          }
          last_down = current_time;
          output_count_down++;
        }
      }

      if (op_outputs.contains("o3")) {
        for (const auto& msg : op_outputs["o3"]) {
          uint64_t current_time = msg["time"];
          if (last_ma != static_cast<uint64_t>(-1)) {
            REQUIRE(current_time == last_ma + 1);
          } else {
            first_ma = current_time;
          }
          last_ma = current_time;
          output_count_ma++;
        }
      }
    }

    // Verify output consistency
    REQUIRE(output_count_up == output_count_down);
    REQUIRE(output_count_ma == output_count_down);
    REQUIRE(output_count_up > 0);
    REQUIRE(first_up == first_down);
    REQUIRE(first_up == first_ma);
  }
}