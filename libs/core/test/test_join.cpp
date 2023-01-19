#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/PeakDetector.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"


using namespace rtbot;
using namespace std;

TEST_CASE("Join peak and value")
{
    auto i1 = Input<double>("i1");
    auto peak = PeakDetector("b1", 3);
    auto o1 = makeOutput<double>("o1", cout);
    auto join = Join<double>("j1");

    connect(&i1, &peak);
    connect(&i1, &join);
    connect(&peak ,&join);
    connect(&join, &o1);

    // process the data
    for(int i=0; i<26; i++)
        i1.receive(Message<>(i, i%5));

}
