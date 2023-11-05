#include <catch2/catch.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/Collector.h"
#include "rtbot/FactoryOp.h"
#include "rtbot/Pipeline.h"
#include "rtbot/Output.h"
#include "tools.h"


using namespace rtbot;
using namespace std;

TEST_CASE("Pipeline operator") {

    SECTION("peaks for ppg") {
        nlohmann::json json;
        {
            ifstream in("examples/data/program-test-1.json");
            if (!in) throw runtime_error("file not found");
            in >> json;
        }

        auto s = SamplePPG("examples/data/ppg.csv");
        auto pipe1 = Pipeline<>("pipe", json.dump());
        Pipeline<> pipe = pipe1;
        Output<uint64_t,double> out1("out1");
        pipe.connect(out1,"o1");            // to test the connection

        // process the data
        for (auto i = 0u; i < s.ti.size(); i++) {
            pipe.receiveData(Message {uint64_t(s.ti[i]), s.ppg[i]}, "i1");
            auto out=pipe.executeData();
            if (out.empty()) continue;
            if (auto it=out.at(pipe.id).find("o1"); it!=out.at(pipe.id).end()) {
                auto msg=it->second.at(0);
                cout << msg.time<<" "<<msg.value << endl;
                REQUIRE(out.count(out1.id));    // to test the connection
            }
        }
    }

    SECTION("peaks for basic data y=x%5") {
        nlohmann::json json;
        {
            ifstream in("examples/data/program-test-2.json");
            if (!in) throw runtime_error("file not found");
            in >> json;
        }
        auto pipe = Pipeline<>("pipe", json.dump());

        // pass through the factory
        string s=FactoryOp::writeOp(make_unique<Pipeline<>>(pipe));
        cout<<"Pipeline to json:\n"<< s << endl;
        auto op=FactoryOp::readOp(s);

        // process the data
        for (int i = 0; i < 100; i++) {
            op->receiveData(Message<uint64_t,double>(i, i % 5), "i1");
            auto output=op->executeData();

            if (i >= 5 && i % 5 == 0) {
                REQUIRE(output.find("pipe")->second.find("o1")->second.size() == 1);
                REQUIRE(output.find("pipe")->second.find("o1")->second.at(0).value == 4);
                REQUIRE(output.find("pipe")->second.find("o1")->second.at(0).time == i - 1);

                REQUIRE(output.find("pipe")->second.find("o2")->second.size() == 1);
                REQUIRE(output.find("pipe")->second.find("o2")->second.at(0).value == 4);
                REQUIRE(output.find("pipe")->second.find("o2")->second.at(0).time == i - 1);
            } else if (!output.empty()) {
                REQUIRE(output.find("pipe")->second.count("join") == 0);
            }
        }
    }
}

#include "rtbot/Composite.h"

TEST_CASE("Composite replacement") {
  double n = 14;
  auto composite = Composite<uint64_t, double>("cmp");
  using nlohmann::json;
  auto c1 = Collector<uint64_t, double>("c1", 50);
  auto c2 = Collector<uint64_t, double>("c2", 50);
  nlohmann::json j;
  j["operators"]=
  {
          {{"type","Input"}, {"id", "in"}},
          {{"type","Demultiplexer"}, {"id", "dm"}, {"numOutputPorts", 2}},
          {{"type","Count"}, {"id", "count"}},
          {{"type","LessThan"}, {"id", "lt"}, {"value", n + 1.0}},
          {{"type","EqualTo"}, {"id", "et"}, {"value", n + 1.0}},
          {{"type","GreaterThan"}, {"id", "gt"}, {"value", n + 1.0}},
          {{"type","EqualTo"}, {"id", "etn2"}, {"value", n + 2.0}},

          {{"type","Constant"}, {"id", "cgtz"}, {"value", 0}},
          {{"type","Constant"}, {"id", "cgto"}, {"value", 1}},
          {{"type","Constant"}, {"id", "cltz"}, {"value", 0}},
          {{"type","Constant"}, {"id", "clto"}, {"value", 1}},
          {{"type","Constant"}, {"id", "cetz"}, {"value", 0}},
          {{"type","Constant"}, {"id", "ceto"}, {"value", 1}},

          {{"type","Difference"}, {"id", "diff1"}},
          {{"type","Difference"}, {"id", "diff2"}},
          {{"type","LessThan"}, {"id", "lt0"}, {"value", 0.0}},
          {{"type","EqualTo"}, {"id", "et0"}, {"value", 0.0}},
          {{"type","GreaterThan"}, {"id", "gt0"}, {"value", 0.0}},
          {{"type","CumulativeSum"}, {"id", "sum0"}},
          {{"type","Scale"}, {"id", "sc0"}, {"value", 1.0 / n}},
            composite.createLinear("l1", {1.0 * (n - 1) / n, 1.0 / n});
            composite.createScale("neg0", -1.0);
            composite.createCumulativeSum("sum1");
            composite.createScale("sc1", 1.0 / n);
            composite.createLinear("l2", {1.0 * (n - 1) / n, 1.0 / n});
  };

}
