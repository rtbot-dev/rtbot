#include <catch2/catch.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/bindings.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("read  pipeline test bollinger_bands") {
  nlohmann::json json;
  {
    ifstream in("examples/data/program-test-6.json");
    if (!in) throw runtime_error("file not found");
    in >> json;
  }

  SECTION("test generated bollinger_bands") {
    auto pipe = FactoryOp::createProgram(json.dump().c_str());

    srand(time(nullptr));
    vector<int> v;
    int start = 0;
    int end = 10000;
    int o1 = 0;
    int o2 = 0;
    int o3 = 0;
    int firstdown = 0;
    int firstma = 0;
    int firstup = 0;
    int lastdown = -1;
    int lastup = -1;
    int lastma = -1;
    v.push_back(start);
    for (int i = start + 1; i < end; i++) {
      int random_value = rand();
      if (random_value < (RAND_MAX / 2)) {
        v.push_back(i);
      }
    }
    v.push_back(end);

    for (int i = 0; i < v.size(); i++) {
      PortPayload<uint64_t, double> messagesMap;
      vector<Message<uint64_t, double>> x;
      x.push_back(Message<uint64_t, double>(v.at(i), v.at(i) * v.at(i)));
      messagesMap.emplace("i1", x);

      auto output = pipe.receive(messagesMap);

      if (output.count("37") > 0) {
        if (output.find("37")->second.count("o1") > 0) {
          for (int j = 0; j < output.find("37")->second.find("o1")->second.size(); j++) {
            int now = output.find("37")->second.find("o1")->second.at(j).time;
            if (lastup >= 0)
              REQUIRE(now - 1 == lastup);
            else
              firstup = now;
            lastup = now;
            o1++;
          }
        }
        if (output.find("37")->second.count("o2") > 0) {
          for (int j = 0; j < output.find("37")->second.find("o2")->second.size(); j++) {
            int now = output.find("37")->second.find("o2")->second.at(j).time;
            if (lastdown >= 0)
              REQUIRE(now - 1 == lastdown);
            else
              firstdown = now;
            lastdown = now;
            o2++;
          }
        }
        if (output.find("37")->second.count("o3") > 0) {
          for (int j = 0; j < output.find("37")->second.find("o3")->second.size(); j++) {
            int now = output.find("37")->second.find("o3")->second.at(j).time;
            if (lastma >= 0)
              REQUIRE(now - 1 == lastma);
            else
              firstma = now;
            lastma = now;
            o3++;
          }
        }
      }
    }
    REQUIRE(o1 == o2);
    REQUIRE(o3 == o2);
    REQUIRE(o1 > 0);
    REQUIRE(firstup == firstdown);
    REQUIRE(firstup == firstma);
  }
}
