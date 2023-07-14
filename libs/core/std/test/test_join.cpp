#include <catch2/catch.hpp>

#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/PeakDetector.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Join peak and value") {
  auto in1 = Input<uint64_t, double>("in");
  auto peak = PeakDetector<uint64_t, double>("peak", 3);
  auto join = Join<uint64_t, double>("join", 2);

  in1.connect(peak)->connect(join, "o1", "i1");
  in1.connect(join, "o1", "i2");

  // process the data
  SECTION("Join output size should be 2") {
    for (int i = 0; i < 100; i++) {
      auto output = in1.receiveData(Message<uint64_t, double>(i, i % 5));
      if (i > 5 && i % 5 == 1) {
        REQUIRE(output.find("join")->second.find("o1")->second.size() == 1);
        REQUIRE(output.find("join")->second.find("o1")->second.at(0).value == 4);
        REQUIRE(output.find("join")->second.find("o1")->second.at(0).time == i - 2);

        REQUIRE(output.find("join")->second.find("o2")->second.size() == 1);
        REQUIRE(output.find("join")->second.find("o2")->second.at(0).value == 4);
        REQUIRE(output.find("join")->second.find("o2")->second.at(0).time == i - 2);
      } else {
        REQUIRE(output.count("join") == 0);
      }
    }
  }
}
