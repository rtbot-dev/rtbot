#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/bindings.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("read ppg pipeline") {
  nlohmann::json json;
  {
    ifstream in("examples/data/program-test-1.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  auto s = SamplePPG("examples/data/ppg.csv");

  SECTION("using the pipeline") {
    auto pipe = FactoryOp::createProgram(json.dump().c_str());

    // process the data
    for (auto i = 0u; i < s.ti.size(); i++) {
      OperatorMessage<uint64_t, double> messagesMap;
      vector<Message<uint64_t, double>> v;
      v.push_back(Message<uint64_t, double>(s.ti[i], s.ppg[i]));
      messagesMap.emplace("i1", v);

      pipe.receive(messagesMap);
    }
  }

  SECTION("using the bindings and  message buffer comparison") {
    createProgram("pipe1", json.dump());
    createProgram("pipe2", json.dump());
    string entryPipe1 = getProgramEntryOperatorId("pipe1");
    string entryPipe2 = getProgramEntryOperatorId("pipe2");
    REQUIRE(entryPipe1 == entryPipe2);
    REQUIRE(entryPipe1 == "in1");
    // process the data
    for (auto i = 0u; i < s.ti.size(); i++) {
      OperatorMessage<uint64_t, double> messagesMap;
      vector<Message<uint64_t, double>> v;
      v.push_back(Message<uint64_t, double>(s.ti[i], s.ppg[i]));
      messagesMap.emplace("i1", v);
      string pipe1result = processMessageMap("pipe1", messagesMap);
      addToMessageBuffer("pipe2", "i1", s.ti[i], s.ppg[i]);
      string pipe2result = processMessageBuffer("pipe2");

      REQUIRE(pipe1result == pipe2result);
    }
  }
}

TEST_CASE("read  pipeline test data basic data") {
  nlohmann::json json;
  {
    ifstream in("examples/data/program-test-2.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  SECTION("using the pipeline") {
    auto pipe = FactoryOp::createProgram(json.dump().c_str());

    // process the data
    for (int i = 0; i < 100; i++) {
      OperatorMessage<uint64_t, double> messagesMap;
      vector<Message<uint64_t, double>> v;
      v.push_back(Message<uint64_t, double>(i, i % 5));
      messagesMap.emplace("i1", v);
      auto output = pipe.receiveDebug(messagesMap);

      if (i >= 5 && i % 5 == 0) {
        REQUIRE(output.find("join")->second.find("o1")->second.size() == 1);
        REQUIRE(output.find("join")->second.find("o1")->second.at(0).value == 4);
        REQUIRE(output.find("join")->second.find("o1")->second.at(0).time == i - 1);

        REQUIRE(output.find("join")->second.find("o2")->second.size() == 1);
        REQUIRE(output.find("join")->second.find("o2")->second.at(0).value == 4);
        REQUIRE(output.find("join")->second.find("o2")->second.at(0).time == i - 1);
      } else {
        REQUIRE(output.count("join") == 0);
      }
    }
  }
}

TEST_CASE("read  pipeline test join port") {
  nlohmann::json json;
  {
    ifstream in("examples/data/program-test-3.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  SECTION("using the pipeline") {
    auto pipe = FactoryOp::createProgram(json.dump().c_str());

    // process the data
    for (int i = 1; i < 100; i++) {
      OperatorMessage<uint64_t, double> messagesMap;
      vector<Message<uint64_t, double>> v;
      v.push_back(Message<uint64_t, double>(i, i % 5));
      messagesMap.emplace("i1", v);
      auto output = pipe.receiveDebug(messagesMap);

      if (i >= 2) {
        REQUIRE(output.find("sc1")->second.find("o1")->second.size() == 1);
        REQUIRE(output.find("sc1")->second.find("o1")->second.at(0).time == i);
        REQUIRE(output.find("sc1")->second.find("o1")->second.at(0).value == 2 * (i % 5));

        REQUIRE(output.find("sc2")->second.find("o1")->second.size() == 1);
        REQUIRE(output.find("sc2")->second.find("o1")->second.at(0).time == i);
        REQUIRE(output.find("sc2")->second.find("o1")->second.at(0).value == 3 * (i % 5));
      }
    }
  }
}