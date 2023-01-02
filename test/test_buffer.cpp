#include<catch2/catch.hpp>
#include"rtbot/Buffer.h"
#include<iostream>

using namespace rtbot;
using namespace std;

TEST_CASE("Buffer")
{
    int dim=3, nlag=2;
    auto msg=Buffer<double>(dim,nlag);

    SECTION("constructor")
    {
        REQUIRE(msg.channelSize==dim);
        REQUIRE(msg.windowSize==nlag);
    }

    SECTION("add")
    {
        REQUIRE(msg.getData().empty());
        msg.add({1,2,3});
        vector<vector<double>> out={{1,2,3}};
        REQUIRE(msg.getData()==out);
        msg.add({4,5,6});
        out={{1,2,3},{4,5,6}};
        REQUIRE(msg.getData()==out);
        msg.add({6,7,8});
        out={{4,5,6},{6,7,8}};
        REQUIRE(msg.getData()==out);
    }

}
