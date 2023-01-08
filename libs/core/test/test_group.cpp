#include "rtbot/Group.h"
#include "rtbot/Output.h"
#include <catch2/catch.hpp>
#include <iostream>

using namespace rtbot;
using namespace std;


TEST_CASE("Group")
{
    int dim = 1, nlag = 2;
    auto op = Group<double>("g1", dim, nlag);

    SECTION("constructor") {
        REQUIRE(op.id=="g1");
    }

    auto output=makeOutput<double>("o1",cout);
    op.addChildren(&output);

    SECTION("Output") {
        for(int i=0; i<5; i++)
            op.receive(i, Buffer<double>(1,1,{1.0*i}));

    }
}

