#include <algorithm>
#include <catch2/catch.hpp>

#include "rtbot/Collector.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/Minus.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "tools.h"

using namespace rtbot;
using namespace std;

TEST_CASE("simple peak detector") {
  int nlag = 3;

  SECTION("one peak") {
    auto op = PeakDetector<uint64_t, double>("b1", nlag);
    auto c1 = Collector<uint64_t, double>("c1", 2);
    REQUIRE(op.connect(c1) != nullptr);
    for (int i = 0; i < 10; i++) op.receiveData(Message<uint64_t, double>(i, 5 - fabs(1.0 * i - 5)));
    REQUIRE(c1.getDataInputSize("i1") == 1);
    REQUIRE(c1.getDataInputMessage("i1", 0) == Message<uint64_t, double>(5, 5.0));
  }

  SECTION("two peaks") {
    auto op = PeakDetector<uint64_t, double>("b1", nlag);
    auto c1 = Collector<uint64_t, double>("c1", 2);
    REQUIRE(op.connect(c1) != nullptr);
    for (int i = 0; i < 14; i++) op.receiveData(Message<uint64_t, double>(i, i % 5));
    REQUIRE(c1.getDataInputSize("i1") == 2);
    REQUIRE(c1.getDataInputMessage("i1", 0) == Message<uint64_t, double>(4, 4.0));
    REQUIRE(c1.getDataInputMessage("i1", 1) == Message<uint64_t, double>(9, 4.0));
  }

  SECTION("only one peak") {
    int t = 0;
    int sign = 1;
    double v = 0.0;

    map<string, map<string, vector<Message<uint64_t, double>>>> emitted;
    auto pd = PeakDetector<uint64_t, double>("b2", nlag);
    for (int i = 1; i <= 10; i++) {
      t++;
      v += sign * 0.1;
      if (t % 5 == 0) sign = -sign;
      emitted = pd.receiveData(Message<uint64_t, double>(i, v));
      if (i < 6) {
        REQUIRE(emitted.empty());
      } else if (i == 6) {
        REQUIRE(emitted.find("b2")->second.find("o1")->second.at(0).value == 0.5);
        REQUIRE(emitted.find("b2")->second.find("o1")->second.at(0).time == 5);
        REQUIRE(emitted.find("b2")->second.size() == 1);
      } else if (i > 6) {
        REQUIRE(emitted.empty());
      }
    }
  }
}

TEST_CASE("ppg peak detector") {
  auto s = SamplePPG("examples/data/ppg.csv");

  auto i1 = Input<uint64_t, double>("i1");
  auto ma1 = MovingAverage<uint64_t, double>("ma1", round(50 / s.dt()));
  auto ma2 = MovingAverage<uint64_t, double>("ma2", round(2000 / s.dt()));
  auto diff = Minus<uint64_t, double>("diff");
  auto peak = PeakDetector<uint64_t, double>("b1", 2 * ma1.getDataInputMaxSize() + 1);
  auto join = Join<uint64_t, double>("j1", 2);
  ofstream out("peak.txt");
  auto o1 = Output_os<uint64_t, double>("o1", out);

  // draw the pipeline

  i1.connect(ma1)
      ->connect(diff, "o1", "i1")
      ->connect(peak, "o1", "i1")
      ->connect(join, "o1", "i1")
      ->connect(o1, "o1", "i1");
  i1.connect(ma2)->connect(diff, "o1", "i2");
  i1.connect(join, "o1", "i2");

  // process the data
  for (auto i = 0u; i < s.ti.size(); i++) i1.receiveData(Message<uint64_t, double>(s.ti[i], s.ppg[i]));
}
