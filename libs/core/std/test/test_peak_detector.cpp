#define CATCH_CONFIG_MAIN
#include <algorithm>
#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/Difference.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("simple peak detector") {
  int nlag = 3;
  auto op = PeakDetector<std::uint64_t, double>("b1", nlag);

  vector<Message<std::uint64_t, double>> msg_l;
  auto o1 = Output_vec<std::uint64_t, double>("o1", msg_l);
  op.connect(o1);

  SECTION("one peak") {
    for (int i = 0; i < 10; i++) op.receive(Message<std::uint64_t, double>(i, 5 - fabs(1.0 * i - 5)));
    REQUIRE(msg_l.size() == 1);
    REQUIRE(msg_l[0] == Message<std::uint64_t, double>(5, 5.0));
  }

  SECTION("two peaks") {
    for (int i = 0; i < 14; i++) op.receive(Message<std::uint64_t, double>(i, i % 5));
    REQUIRE(msg_l.size() == 2);
    REQUIRE(msg_l[0] == Message<std::uint64_t, double>(4, 4.0));
    REQUIRE(msg_l[1] == Message<std::uint64_t, double>(9, 4.0));
  }
}

TEST_CASE("ppg peak detector") {
  auto s = SamplePPG("examples/data/ppg.csv");

  auto i1 = Input<std::uint64_t, double>("i1");
  auto ma1 = MovingAverage<std::uint64_t, double>("ma1", round(50 / s.dt()));
  auto ma2 = MovingAverage<std::uint64_t, double>("ma2", round(2000 / s.dt()));
  auto diff = Difference<std::uint64_t, double>("diff");
  auto peak = PeakDetector<std::uint64_t, double>("b1", 2 * ma1.n + 1);
  auto join = Join<std::uint64_t, double>("j1", 2);
  ofstream out("peak.txt");
  auto o1 = Output_os<std::uint64_t, double>("o1", out);

  // draw the pipeline

  i1.connect(ma1).connect(diff, 0).connect(peak).connect(join, 0).connect(o1, 0, 1);
  i1.connect(ma2).connect(diff, 1);
  i1.connect(join, 1);

  // process the data
  for (auto i = 0u; i < s.ti.size(); i++) i1.receive(Message<std::uint64_t, double>(s.ti[i], s.ppg[i]));
}
