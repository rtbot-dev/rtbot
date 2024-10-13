#include <catch2/catch.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/bindings.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("read  pipeline test rsi") {
  nlohmann::json json;
  {
    ifstream in("examples/data/program-test-5.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  SECTION("test generated rsi") {
    auto pipe = FactoryOp::createProgram(json.dump().c_str());

    std::vector<double> values = {54.8,  56.8,  57.85, 59.85, 60.57, 61.1,  62.17, 60.6,  62.35, 62.15,
                                  62.35, 61.45, 62.8,  61.37, 62.5,  62.57, 60.8,  59.37, 60.35, 62.35,
                                  62.17, 62.55, 64.55, 64.37, 65.3,  64.42, 62.9,  61.6,  62.05, 60.05,
                                  59.7,  60.9,  60.25, 58.27, 58.7,  57.72, 58.1,  58.2};

    std::vector<double> rsis = {0,        0,        0,        0,        0,        0,        0,        0,
                                0,        0,        0,        0,        0,        0,        74.21384, 74.33552,
                                65.87129, 59.93370, 62.43288, 66.96205, 66.18862, 67.05377, 71.22679, 70.36299,
                                72.23644, 67.86486, 60.99822, 55.79821, 57.15964, 49.81579, 48.63810, 52.76154,
                                50.40119, 43.95111, 45.57992, 42.54534, 44.09946, 44.52472};

    // process the data
    int firstTime = 15;
    for (int i = 1; i < values.size(); i++) {
      OperatorMessage<uint64_t, double> messagesMap;
      vector<Message<uint64_t, double>> v;
      v.push_back(Message<uint64_t, double>(i, values.at(i - 1)));
      messagesMap.emplace("i1", v);
      auto output = pipe.receiveDebug(messagesMap);

      if (output.count("467") > 0) {
        for (int j = 0; j < output.find("467")->second.find("o1")->second.size(); j++) {
          REQUIRE(output.find("467")->second.find("o1")->second.at(j).time == firstTime);
          REQUIRE(abs(output.find("467")->second.find("o1")->second.at(j).value - rsis.at(firstTime - 1)) < 0.00001);
          firstTime++;
        }
      }
    }
  }
}
