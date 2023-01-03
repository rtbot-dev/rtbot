#include "rtbot/Buffer.h"
#include <catch2/catch.hpp>
#include <iostream>

using namespace rtbot;
using namespace std;

TEST_CASE("Buffer") {
  int dim = 3, nlag = 2;
  auto msg = Buffer<double>(dim, nlag);

  SECTION("constructor") {
    REQUIRE(msg.channelSize == dim);
    REQUIRE(msg.windowSize == nlag);
  }

  SECTION("add") {
    REQUIRE(msg.getData().empty());
    vector<double> a,b;
    msg.add(a={1, 1, 1});
    REQUIRE(msg.getData() == vector<vector<double>>({a}));
    for(double i=2; i<10; i++) {
        msg.add(b={i, i, i});
        REQUIRE(msg.getData() == vector<vector<double>>( {a,b} ) );
        a=b;
    }
  }
}
