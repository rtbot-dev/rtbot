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
    auto i1 = Input("i1",Type::cosine,3);
    auto peak = PeakDetector("b1", 3);
    auto o1 = Output_os("o1",std::cout);
    auto join = Join<double>("j1",2);

    i1.connect(peak).connect(join,0).connect(o1) ;
    i1.connect(join,1) ;

    // process the data
    for(int i=0; i<26; i++)
        i1.receive( Message<>(i, i%5) );

}
