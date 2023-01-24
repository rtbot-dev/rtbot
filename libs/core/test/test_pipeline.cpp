#include <catch2/catch.hpp>
#include "rtbot/FactoryOp.h"
#include "tools.h"

#include <fstream>

using namespace rtbot;
using namespace std;


TEST_CASE("read ppg pipeline")
{
    nlohmann::json json;
    {
        ifstream in("ppg.json");
        in>>json;
    }

    auto pipe = FactoryOp::createPipeline(json);
    auto s=SamplePPG("ppg.csv");

    // process the data
    for(auto i=0u; i<s.ti.size(); i++)
        pipe.receive( Message<>(s.ti[i], s.ppg[i]) );
}
