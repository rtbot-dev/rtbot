#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/tools/PeakDetector.h"
#include "rtbot/tools/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"


using namespace rtbot;
using namespace std;

TEST_CASE("Join peak and value")
{
    auto i1 = InputCosine<double>("i1");
    auto peak = PeakDetector("b1", 3);
    auto o1 = Output_os("o1",std::cout);
    auto join = Join<double>("j1");

    i1 | peak | join | o1 ;
    i1        | join ;

    // process the data
    for(int i=0; i<26; i++)
        Message<>(i, i%5) | i1;

}
