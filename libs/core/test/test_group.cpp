#include "rtbot/Buffer.h"
#include "rtbot/Output.h"
#include <catch2/catch.hpp>
#include <iostream>
#include <algorithm>

using namespace rtbot;
using namespace std;


struct SumOp: Buffer<double>
{
    using Buffer<double>::Buffer;

    Message<> compute() const override
    {
        Message<> out=at(n);
        for(auto i=0;i<n-1; i++)
            for(auto j=0u; j<this->at(i).value.size(); j++)
                out.value[j] += at(i).value[j];
        return out;
    }
};


TEST_CASE("Group")
{
    int nlag = 2;
    auto op = SumOp("g1", nlag);

    SECTION("constructor") {
        REQUIRE(op.id=="g1");
    }

    auto output=makeOutput<double>("o1",cout);
    op.addChildren(&output);

    SECTION("Output") {
        for(int i=0; i<5; i++)
            op.receive(Message<> {i, {1.0*i}});

    }
}

