#include "rtbot/Buffer.h"
#include "rtbot/Output.h"
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

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
    for(int i=2; i<10.0; i++) {
        double x=i;
        msg.add(b={x, x, x});
        REQUIRE(msg.getData() == vector<vector<double>>( {a,b} ) );
        a=b;
    }
  }

  auto output=makeOutput<double>("o1",cout);

  SECTION("Output") {
      output.receive(1,msg);
  }
}
