#include <catch2/catch.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/bindings.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Program collect and restore") {
  nlohmann::json json;
  {
    ifstream in("examples/data/program-test-6.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  SECTION("test fresh vs restored program") {
    auto program = FactoryOp::createProgram(json.dump().c_str());

    srand(time(nullptr));
    vector<int> v;
    int start = 0;
    int end = 10000;
    v.push_back(start);
    for (int i = start + 1; i < end; i++) {
      int random_value = rand();
      if (random_value < (RAND_MAX / 2)) {
        v.push_back(i);
      }
    }
    v.push_back(end);

    uint32_t half = v.size() / 2;

    // send the first half of the data to the first program
    for (int i = 0; i < half; i++) {
      OperatorMessage<uint64_t, double> messagesMap;
      vector<Message<uint64_t, double>> x;
      x.push_back(Message<uint64_t, double>(v.at(i), v.at(i) * v.at(i)));
      messagesMap.emplace("i1", x);

      auto output = program.receive(messagesMap);
    }

    Bytes bytes = program.collect();
    // create a new program
    auto program2 = FactoryOp::createProgram(json.dump().c_str());
    program2.restore(bytes);

    // send the second half of the data
    for (int i = half; i < v.size(); i++) {
      OperatorMessage<uint64_t, double> messagesMap;
      vector<Message<uint64_t, double>> x;
      x.push_back(Message<uint64_t, double>(v.at(i), v.at(i) * v.at(i)));
      messagesMap.emplace("i1", x);

      // send the same data to the two programs
      ProgramMessage<uint64_t, double> output = program.receive(messagesMap);
      ProgramMessage<uint64_t, double> output2 = program2.receive(messagesMap);

      // cout << "output[" << i << "] " << endl << debug(output) << endl;
      // cout << "output2[" << i << "] " << endl << debug(output2) << endl;

      // compare the outputs
      for (auto const& [opId, opMsgs] : output) {
        for (auto const& [portId, portMsgs] : opMsgs) {
          REQUIRE(output2.count(opId) > 0);
          REQUIRE(output2.at(opId).count(portId) > 0);
          REQUIRE(output2.at(opId).at(portId).size() == portMsgs.size());
          for (size_t j = 0; j < portMsgs.size(); j++) {
            REQUIRE(output2.at(opId).at(portId).at(j).time == portMsgs.at(j).time);
            REQUIRE(output2.at(opId).at(portId).at(j).value == portMsgs.at(j).value);
          }
        }
      }
    }

    //
  }
}
