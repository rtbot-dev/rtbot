#include <catch2/catch.hpp>
#include "rtbot/FactoryOp.h"
#include "tools.h"
#include "rtbot/bindings.h"
#include "rtbot/tools/Input.h"

#include <fstream>
#include <nlohmann/json.hpp>

using namespace rtbot;
using namespace std;


TEST_CASE("read ppg pipeline")
{
    nlohmann::json json;
    {
        ifstream in("libs/core/test/ppg.json");
        in>>json;
    }

    auto s=SamplePPG("examples/data/ppg.csv");

    SECTION("using the pipeline")
    {
        auto pipe = FactoryOp::createPipeline(json.dump().c_str());
        std::cout << "iType " << (((Input*)pipe.input)->iType) << std::endl;
        std::cout << "n " << (((Input*)pipe.input)->n) << std::endl;
        std::cout << "dt " << (((Input*)pipe.input)->dt) << std::endl;
        // process the data
        for(auto i=0u; i<s.ti.size(); i++) {

            auto y=pipe.receive( Message<>(s.ti[i], s.ppg[i]) )[0];
            if (y) cout<<y.value()<<"\n";
        }
    }

    SECTION("using the bindings")
    {
        createPipeline("pipe1", json.dump());
        // process the data
        for(auto i=0u; i<s.ti.size(); i++) {
            auto y=receiveMessageInPipeline("pipe1", Message<>(s.ti[i], s.ppg[i]) )[0];
            if (y) cout<<y.value()<<"\n";
        }
    }
}
