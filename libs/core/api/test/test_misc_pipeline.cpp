#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/bindings.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("read ppg pipeline") {
  nlohmann::json json;
  {
    ifstream in("examples/data/ppg-test-1.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  auto s = SamplePPG("examples/data/ppg.csv");

  SECTION("using the pipeline") {
    auto pipe = FactoryOp::createProgram(json.dump().c_str());

    // process the data
    for (auto i = 0u; i < s.ti.size(); i++) {
      auto y = pipe.receive(Message<uint64_t, double>(s.ti[i], s.ppg[i]))[0];
      if (y) cout << y.value() << endl;
    }
  }

  SECTION("using the bindings") {
    createProgram("pipe1", json.dump());
    // process the data
    for (auto i = 0u; i < s.ti.size(); i++) {
      auto y = processMessage("pipe1", Message<uint64_t, double>(s.ti[i], s.ppg[i]))[0];
      if (y) cout << y.value() << endl;
    }
  }
}

TEST_CASE("read  pipeline test data basic data") {
  nlohmann::json json;
  {
    ifstream in("examples/data/ppg-test-2.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  SECTION("using the pipeline") {
    auto pipe = FactoryOp::createProgram(json.dump().c_str());

    // process the data
    for (int i = 0; i < 100; i++) {
      auto output = pipe.receiveDebug(Message<uint64_t, double>(i, i % 5));

      if (i > 5 && i % 5 == 1) {
        REQUIRE(output.find("join")->second.find("o1")->second.size() == 1);
        REQUIRE(output.find("join")->second.find("o1")->second.at(0).value == 4);
        REQUIRE(output.find("join")->second.find("o1")->second.at(0).time == i - 2);

        REQUIRE(output.find("join")->second.find("o2")->second.size() == 1);
        REQUIRE(output.find("join")->second.find("o2")->second.at(0).value == 4);
        REQUIRE(output.find("join")->second.find("o2")->second.at(0).time == i - 2);
      } else {
        REQUIRE(output.count("join") == 0);
      }
    }
  }
}

TEST_CASE("read  pipeline test join eager port") {
  nlohmann::json json;
  {
    ifstream in("examples/data/ppg-test-3.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  SECTION("using the pipeline") {
    auto pipe = FactoryOp::createProgram(json.dump().c_str());

    REQUIRE(pipe.all_op.find("join")->second->isDataInputEager("i1"));
    REQUIRE(!pipe.all_op.find("join")->second->isDataInputEager("i2"));

    // process the data
    for (int i = 1; i < 100; i++) {
      auto output = pipe.receiveDebug(Message<uint64_t, double>(i, i % 5));

      if (i >= 2) {
        REQUIRE(output.find("sc1")->second.find("o1")->second.size() == 1);
        REQUIRE(output.find("sc1")->second.find("o1")->second.at(0).time == i - 1);
        REQUIRE(output.find("sc1")->second.find("o1")->second.at(0).value == 2 * ((i - 1) % 5));

        REQUIRE(output.find("sc2")->second.find("o1")->second.size() == 1);
        REQUIRE(output.find("sc2")->second.find("o1")->second.at(0).time == i - 1);
        REQUIRE(output.find("sc2")->second.find("o1")->second.at(0).value == 3 * ((i - 1) % 5));
      }
    }

    cout << pipe.getProgram() << endl;
  }
}
