#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/Input.h"
#include "rtbot/std/PeakDetector.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Join peak and value")
{
    auto i1 = Input("i1");
    auto peak = PeakDetector("b1", 3);    
    auto join = Join<double>("j1",2);

  i1.connect(peak).connect(join, 0);
  i1.connect(join, 1);

  // process the data
  SECTION("Join output size should be 2") {
    for (int i = 0; i < 26; i++) {
      auto output = i1.receive(Message<>(i, i % 5));
      if (output.find("j1") != output.end()) {
        REQUIRE(output["j1"][0].value.size() == 2);
      }
    }
  }
}
