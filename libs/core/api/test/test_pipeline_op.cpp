#include <catch2/catch.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/Collector.h"
#include "rtbot/FactoryOp.h"
#include "rtbot/Pipeline.h"
#include "rtbot/Output.h"
#include "rtbot/finance/RelativeStrengthIndex.h"
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

TEST_CASE("Composite replacement") {
  double n = 14;
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
          //{{"type","Constant"}, {"id", "cetz"}, {"value", 0}}, //<-------------- not used
          {{"type","Constant"}, {"id", "ceto"}, {"value", 1}},

          {{"type","Difference"}, {"id", "diff1"}},
          {{"type","Difference"}, {"id", "diff2"}},
          {{"type","LessThan"}, {"id", "lt0"}, {"value", 0.0}},
          {{"type","EqualTo"}, {"id", "et0"}, {"value", 0.0}},
          {{"type","GreaterThan"}, {"id", "gt0"}, {"value", 0.0}},
          {{"type","CumulativeSum"}, {"id", "sum0"}},
          {{"type","Scale"}, {"id", "sc0"}, {"value", 1.0 / n}},
          {{"type","Linear"}, {"id", "l1"}, {"coeff", {1.0 * (n - 1) / n, 1.0 / n}}},
          {{"type","Scale"}, {"id", "neg0"}, {"value", -1.0}},
          {{"type","CumulativeSum"}, {"id", "sum1"}},
          {{"type","Scale"}, {"id", "sc1"}, {"value", 1.0 / n}},
          {{"type","Linear"}, {"id", "l2"}, {"coeff", {1.0 * (n - 1) / n, 1.0 / n}}},

          {{"type","LessThan"}, {"id", "lt1"}, {"value", 0.0}},
          {{"type","EqualTo"}, {"id", "et1"}, {"value", 0.0}},
          {{"type","GreaterThan"}, {"id", "gt1"}, {"value", 0.0}},

          {{"type","Constant"}, {"id", "const0"}, {"value", 0}},
          {{"type","Scale"}, {"id", "neg1"}, {"value", -1.0}},
          {{"type","Constant"}, {"id", "const1"}, {"value", 0}},

          {{"type","Variable"}, {"id", "varg"}},
          {{"type","Variable"}, {"id", "varl"}},
          {{"type","TimeShift"}, {"id", "ts1"}},
          {{"type","TimeShift"}, {"id", "ts2"}},

          {{"type","Division"}, {"id", "divide"}},
          {{"type","Add"}, {"id", "add1"}, {"value", 1.0}},
          {{"type","Power"}, {"id", "-power1"}, {"value", -1.0}},
          {{"type","Scale"}, {"id", "-scale100"}, {"value", -100}},
          {{"type","Add"}, {"id", "add100"}, {"value", 100.0}},
          {{"type","Output"}, {"id", "out"}}
  };

  auto myconnect=[&](string from, string to, string fromPort, string toPort) {
      j["connections"].push_back({{"from",from}, {"to",to}, {"fromPort", fromPort}, {"toPort",toPort}});
  };

  {// connect
      myconnect("in", "dm", "o1", "i1");
      /*** multiplexer setup ****/
      myconnect("in", "count", "o1", "i1");
      myconnect("count", "lt", "o1", "i1");
      myconnect("count", "gt", "o1", "i1");
      myconnect("count", "et", "o1", "i1");
      myconnect("count", "etn2", "o1", "i1");

      myconnect("lt", "clto", "o1", "i1");
      myconnect("clto", "dm", "o1", "c1");
      myconnect("lt", "cltz", "o1", "i1");
      myconnect("cltz", "dm", "o1", "c2");

      myconnect("et", "ceto", "o1", "i1");
      myconnect("ceto", "dm", "o1", "c1");
      myconnect("ceto", "dm", "o1", "c2");

      myconnect("gt", "cgto", "o1", "i1");
      myconnect("gt", "cgtz", "o1", "i1");
      myconnect("cgto", "dm", "o1", "c2");
      myconnect("cgtz", "dm", "o1", "c1");
      /*** multiplexer setup ****/

      /*** first, second and third route ****/
      myconnect("dm", "diff1", "o1", "i1");
      /*** first, second and third route ****/

      /*** first route ***/
      myconnect("diff1", "gt0", "o1", "i1");
      myconnect("gt0", "sum0", "o1", "i1");
      myconnect("sum0", "sc0", "o1", "i1");
      myconnect("sc0", "varg", "o1", "i1");
      myconnect("varg", "ts1", "o1", "i1");
      myconnect("ts1", "l1", "o1", "i1");
      myconnect("l1", "ts1", "o1", "i1");
      /*** first route ***/

      /*** second route ***/
      myconnect("diff1", "et0", "o1", "i1");
      myconnect("et0", "sum0", "o1", "i1");
      myconnect("et0", "sum1", "o1", "i1");
      /*** second route ***/

      /*** third route ***/
      myconnect("diff1", "lt0", "o1", "i1");
      myconnect("lt0", "neg0", "o1", "i1");
      myconnect("neg0", "sum1", "o1", "i1");
      myconnect("sum1", "sc1", "o1", "i1");
      myconnect("sc1", "varl", "o1", "i1");
      myconnect("varl", "ts2", "o1", "i1");
      myconnect("ts2", "l2", "o1", "i1");
      myconnect("l2", "ts2", "o1", "i1");
      /*** third route ***/

      /*** first, second and third route ****/
      myconnect("dm", "diff2", "o2", "i1");
      /*** first, second and third route ****/

      /*** first route ***/
      myconnect("diff2", "gt1", "o1", "i1");
      myconnect("gt1", "l1", "o1", "i2");
      myconnect("gt1", "const0", "o1", "i1");
      myconnect("const0", "l2", "o1", "i2");
      /*** first route ***/

      /*** second route ***/
      myconnect("diff2", "lt1", "o1", "i1");
      myconnect("lt1", "neg1", "o1", "i1");
      myconnect("neg1", "l2", "o1", "i2");
      myconnect("lt1", "const1", "o1", "i1");
      myconnect("const1", "l1", "o1", "i2");
      /*** second route ***/

      /*** third route ***/
      myconnect("diff2", "et1", "o1", "i1");
      myconnect("et1", "l1", "o1", "i2");
      myconnect("et1", "l2", "o1", "i2");
      /*** third route ***/

      /*** first solution flow ***/

      myconnect("etn2", "varg", "o1", "i1");
      myconnect("et", "varg", "o1", "c1");
      myconnect("etn2", "varl", "o1", "i1");
      myconnect("et", "varl", "o1", "c1");

      myconnect("varg", "divide", "o1", "i1");
      myconnect("varl", "divide", "o1", "i2");
      /*** first solution flow ***/

      /*** final route ***/
      myconnect("l1", "divide", "o1", "i1");
      myconnect("l2", "divide", "o1", "i2");
      myconnect("divide", "add1", "o1", "i1");
      myconnect("add1", "-power1", "o1", "i1");
      myconnect("-power1", "-scale100", "o1", "i1");
      myconnect("-scale100", "add100", "o1", "i1");
      myconnect("add100", "out", "o1", "i1");
      /*** final route ***/
  }

  auto composite=Pipeline<>("pipe", j.dump());
  composite.connect(c1);

  std::vector<double> values = {54.8,  56.8,  57.85, 59.85, 60.57, 61.1,  62.17, 60.6,  62.35, 62.15,
                                62.35, 61.45, 62.8,  61.37, 62.5,  62.57, 60.8,  59.37, 60.35, 62.35,
                                62.17, 62.55, 64.55, 64.37, 65.3,  64.42, 62.9,  61.6,  62.05, 60.05,
                                59.7,  60.9,  60.25, 58.27, 58.7,  57.72, 58.1,  58.2};

  std::vector<double> rsis = {0,        0,        0,        0,        0,        0,        0,        0,
                              0,        0,        0,        0,        0,        0,        74.21384, 74.33552,
                              65.87129, 59.93370, 62.43288, 66.96205, 66.18862, 67.05377, 71.22679, 70.36299,
                              72.23644, 67.86486, 60.99822, 55.79821, 57.15964, 49.81579, 48.63810, 52.76154,
                              50.40119, 43.95111, 45.57992, 42.54534, 44.09946, 44.52472};

  auto rsi = RelativeStrengthIndex<std::uint64_t, double>("rsi", 14);

  rsi.connect(c2);

  SECTION("Emit rsi") {
      for (size_t i = 0; i < values.size(); i++) {
        rsi.receiveData(Message<uint64_t, double>(i + 1, values.at(i)));
        rsi.executeData();
        composite.receiveData(Message<uint64_t, double>(i + 1, values.at(i)));
        composite.executeData();
      }

      REQUIRE(c1.getDataInputSize("i1") > 0);
      REQUIRE(c1.getDataInputSize("i1") == c2.getDataInputSize("i1"));

      for (size_t i = 0; i < c1.getDataInputSize("i1"); i++) {
        REQUIRE(abs(c1.getDataInputMessage("i1", i).value - c2.getDataInputMessage("i1", i).value) < 0.00000001);
        REQUIRE(c1.getDataInputMessage("i1", i).time == c2.getDataInputMessage("i1", i).time);
      }
    }
}
