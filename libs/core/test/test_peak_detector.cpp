#include "rtbot/PeakDetector.h"
#include "rtbot/Output.h"
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
    op.addChildren(&o1);

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

