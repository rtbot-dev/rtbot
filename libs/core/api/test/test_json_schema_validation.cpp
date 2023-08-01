#include <catch2/catch.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/bindings.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("jsonschema validation") {
  SECTION("validates a valid program") {
    nlohmann::json program;
    {
      ifstream in("examples/data/ppg-test-1.json");
      if (!in) throw runtime_error("file not found");
      in >> program;
    }
    cout << "validating program....." << endl;

    std::string result = validate(program.dump());
    cout << "result " << result << endl;
    REQUIRE(nlohmann::json::parse(result)["valid"]);
  }
  SECTION("validates a valid operator") {
    std::string op = R"""({ "id": "id", "type": "Input" })""";
    std::string result = validateOperator("Input", op);
    REQUIRE(nlohmann::json::parse(result)["valid"]);
  }

  SECTION("fails with an invalid operator") {
    std::string op = R"""({ "type": "Input" })""";
    std::string result = validateOperator("Input", op);
    REQUIRE(!nlohmann::json::parse(result)["valid"]);
  }
}
