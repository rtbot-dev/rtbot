#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/Input.h"
#include "rtbot/Joint.h"
#include "rtbot/Output.h"
#include "rtbot/std/PeakDetector.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Join peak and value") {
  auto in1 = Input<uint64_t, double>("in1");
  auto peak = PeakDetector<uint64_t, double>("b1", 3);
  auto join = Joint<uint64_t, double>("j1", 2);
  auto out1 = Output_vec<uint64_t, double>("out1", 1);
  auto out2 = Output_vec<uint64_t, double>("out2", 1);

  in1.connect(peak)->connect(join, "o1", "i1")->connect(out1, "o1", "i1");
  in1.connect(join, "o1", "i2")->connect(out2, "o2", "i1");

  // process the data
  SECTION("Join output size should be 2") {
    for (int i = 0; i < 100; i++) {
      auto output = in1.receive(Message<uint64_t, double>(i, i % 5));

      if (i > 5 && i % 5 == 1) {
        REQUIRE(output["out1"].size() == 1);
        REQUIRE(output["out1"].at(0).value == 4);
        REQUIRE(output["out1"].at(0).time == i - 2);

        REQUIRE(output["out2"].size() == 1);
        REQUIRE(output["out2"].at(0).value == 4);
        REQUIRE(output["out2"].at(0).time == i - 2);
      } else {
        REQUIRE(output["out1"].size() == 0);
        REQUIRE(output["out2"].size() == 0);
      }
    }
  }
}
