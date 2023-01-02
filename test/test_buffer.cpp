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
        REQUIRE(msg.to_matrix()[0].empty());
        msg.add({1,2,3});
        vector<vector<double>> out={{1},{2},{3}};
        REQUIRE(msg.to_matrix()==out);
        msg.add({4,5,6});
        out={{1,4},{2,5},{3,6}};
        REQUIRE(msg.to_matrix()==out);
        msg.add({6,7,8});
        out={{4,6},{5,7},{6,8}};
        REQUIRE(msg.to_matrix()==out);
    }

}
