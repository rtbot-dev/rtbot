#include <catch2/catch.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"
#include "rtbot/Pipeline.h"
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
        Pipeline<> pipe;
        pipe = pipe1;

        // process the data
        for (auto i = 0u; i < s.ti.size(); i++) {
            pipe.receiveData(Message {uint64_t(s.ti[i]), s.ppg[i]}, "in1:i1");
            auto out=pipe.executeData();
            if (out.empty()) continue;
            if (auto it=out.at(pipe.id).find("out1:o1"); it!=out.at(pipe.id).end()) {
                auto msg=it->second.at(0);
                cout << msg.time<<" "<<msg.value << endl;
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
        SECTION("using the pipeline") {
            auto pipe = Pipeline<>("pipe", json.dump());

            // process the data
            for (int i = 0; i < 100; i++) {
                pipe.receiveData(Message<uint64_t,double>(i, i % 5), "in1:i1");
                auto output=pipe.executeData();

                if (i >= 5 && i % 5 == 0) {
                    REQUIRE(output.find("pipe")->second.find("join:o1")->second.size() == 1);
                    REQUIRE(output.find("pipe")->second.find("join:o1")->second.at(0).value == 4);
                    REQUIRE(output.find("pipe")->second.find("join:o1")->second.at(0).time == i - 1);

                    REQUIRE(output.find("pipe")->second.find("join:o2")->second.size() == 1);
                    REQUIRE(output.find("pipe")->second.find("join:o2")->second.at(0).value == 4);
                    REQUIRE(output.find("pipe")->second.find("join:o2")->second.at(0).time == i - 1);
                } else {
                    REQUIRE(output.find("pipe")->second.count("join") == 0);
                }
            }
        }
    }
}
