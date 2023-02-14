#include "rtbot/tools/PeakDetector.h"
#include "rtbot/Output.h"
#include "rtbot/Join.h"
#include "rtbot/tools/MovingAverage.h"
#include "tools.h"

#include <catch2/catch.hpp>
#include <iostream>
#include <algorithm>

using namespace rtbot;
using namespace std;

TEST_CASE("simple peak detector")
{
    int nlag = 3;
    auto op = PeakDetector("b1", nlag);

    vector<Message<>> msg_l;
    auto o1=Output<double>("o1", [&](Message<double> msg){ msg_l.push_back(msg); });
    connect(&op, &o1);

    SECTION("one peak") {
        for(int i=0; i<10; i++)
            op.receive(Message<>(i, 5-fabs(1.0*i-5)));
        REQUIRE(msg_l.size()==1);
        REQUIRE(msg_l[0] == Message<>(5,5.0));
    }

    SECTION("two peaks") {
        for(int i=0; i<14; i++)
            op.receive(Message<>(i, i%5));
        REQUIRE(msg_l.size()==2);
        REQUIRE(msg_l[0] == Message<>(4,4.0));
        REQUIRE(msg_l[1] == Message<>(9,4.0));
    }
}

TEST_CASE("ppg peak detector")
{
    auto s=SamplePPG("ppg.csv");

    auto i1 = Input<double>("i1");
    auto ma1 = MovingAverage("ma1", round(50/s.dt()) );
    auto ma2 = MovingAverage("ma2", round(2000/s.dt()) );
    auto diff = Difference("diff");
    auto peak = PeakDetector("b1", 2*ma1.n+1);
    auto join = Join<double>("j1");
    auto o1 = Output<double>("o1", "peak.txt");

    // draw the pipeline

    i1 | ma1 | diff | peak | join | o1 ;
    i1 | ma2 | diff ;
    i1 |                     join ;

    // process the data
    for(auto i=0u; i<s.ti.size(); i++)
        Message<>(s.ti[i], s.ppg[i]) | i1;
}
