#include <catch2/catch.hpp>
#include <cmath>

#include "rtbot/Collector.h"
#include "rtbot/Composite.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Composite") {
  double n = 14;
  auto composite = Composite<uint64_t, double>("cmp");
  auto c1 = Collector<uint64_t, double>("c1", 50);
  auto c2 = Collector<uint64_t, double>("c2", 50);
  composite.createInput("in");
  composite.createDemultiplexer("dm", 2);
  composite.createCount("count");
  composite.createLessThan("lt", n + 1.0);
  composite.createEqualTo("et", n + 1.0);
  composite.createGreaterThan("gt", n + 1.0);
  composite.createEqualTo("etn2", n + 2.0);

  composite.createConstant("cgtz", 0);
  composite.createConstant("cgto", 1);
  composite.createConstant("cltz", 0);
  composite.createConstant("clto", 1);
  composite.createConstant("cetz", 0);
  composite.createConstant("ceto", 1);

  composite.createDifference("diff1");
  composite.createDifference("diff2");
  composite.createLessThan("lt0", 0.0);
  composite.createEqualTo("et0", 0.0);
  composite.createGreaterThan("gt0", 0.0);
  composite.createCumulativeSum("sum0");
  composite.createScale("sc0", 1.0 / n);
  composite.createLinear("l1", {1.0 * (n - 1) / n, 1.0 / n});
  composite.createScale("neg0", -1.0);
  composite.createCumulativeSum("sum1");
  composite.createScale("sc1", 1.0 / n);
  composite.createLinear("l2", {1.0 * (n - 1) / n, 1.0 / n});

  composite.createLessThan("lt1", 0);
  composite.createEqualTo("et1", 0);
  composite.createGreaterThan("gt1", 0);

  composite.createConstant("const0", 0);
  composite.createScale("neg1", -1.0);
  composite.createConstant("const1", 0);

  composite.createVariable("varg");
  composite.createVariable("varl");
  composite.createTimeShift("ts1");
  composite.createTimeShift("ts2");

  composite.createDivide("divide");
  composite.createAdd("add1", 1.0);
  composite.createPower("-power1", -1.0);
  composite.createScale("-scale100", -100.0);
  composite.createAdd("add100", 100.0);
  composite.createOutput("out");

  composite.createInternalConnection("in", "dm", "o1", "i1");
  /*** multiplexer setup ****/
  composite.createInternalConnection("in", "count", "o1", "i1");
  composite.createInternalConnection("count", "lt", "o1", "i1");
  composite.createInternalConnection("count", "gt", "o1", "i1");
  composite.createInternalConnection("count", "et", "o1", "i1");
  composite.createInternalConnection("count", "etn2", "o1", "i1");

  composite.createInternalConnection("lt", "clto", "o1", "i1");
  composite.createInternalConnection("clto", "dm", "o1", "c1");
  composite.createInternalConnection("lt", "cltz", "o1", "i1");
  composite.createInternalConnection("cltz", "dm", "o1", "c2");

  composite.createInternalConnection("et", "ceto", "o1", "i1");
  composite.createInternalConnection("ceto", "dm", "o1", "c1");
  composite.createInternalConnection("ceto", "dm", "o1", "c2");

  composite.createInternalConnection("gt", "cgto", "o1", "i1");
  composite.createInternalConnection("gt", "cgtz", "o1", "i1");
  composite.createInternalConnection("cgto", "dm", "o1", "c2");
  composite.createInternalConnection("cgtz", "dm", "o1", "c1");
  /*** multiplexer setup ****/

  /*** first, second and third route ****/
  composite.createInternalConnection("dm", "diff1", "o1", "i1");
  /*** first, second and third route ****/

  /*** first route ***/
  composite.createInternalConnection("diff1", "gt0", "o1", "i1");
  composite.createInternalConnection("gt0", "sum0", "o1", "i1");
  composite.createInternalConnection("sum0", "sc0", "o1", "i1");
  composite.createInternalConnection("sc0", "varg", "o1", "i1");
  composite.createInternalConnection("varg", "ts1", "o1", "i1");
  composite.createInternalConnection("ts1", "l1", "o1", "i1");
  composite.createInternalConnection("l1", "ts1", "o1", "i1");
  /*** first route ***/

  /*** second route ***/
  composite.createInternalConnection("diff1", "et0", "o1", "i1");
  composite.createInternalConnection("et0", "sum0", "o1", "i1");
  composite.createInternalConnection("et0", "sum1", "o1", "i1");
  /*** second route ***/

  /*** third route ***/
  composite.createInternalConnection("diff1", "lt0", "o1", "i1");
  composite.createInternalConnection("lt0", "neg0", "o1", "i1");
  composite.createInternalConnection("neg0", "sum1", "o1", "i1");
  composite.createInternalConnection("sum1", "sc1", "o1", "i1");
  composite.createInternalConnection("sc1", "varl", "o1", "i1");
  composite.createInternalConnection("varl", "ts2", "o1", "i1");
  composite.createInternalConnection("ts2", "l2", "o1", "i1");
  composite.createInternalConnection("l2", "ts2", "o1", "i1");
  /*** third route ***/

  /*** first, second and third route ****/
  composite.createInternalConnection("dm", "diff2", "o2", "i1");
  /*** first, second and third route ****/

  /*** first route ***/
  composite.createInternalConnection("diff2", "gt1", "o1", "i1");
  composite.createInternalConnection("gt1", "l1", "o1", "i2");
  composite.createInternalConnection("gt1", "const0", "o1", "i1");
  composite.createInternalConnection("const0", "l2", "o1", "i2");
  /*** first route ***/

  /*** second route ***/
  composite.createInternalConnection("diff2", "lt1", "o1", "i1");
  composite.createInternalConnection("lt1", "neg1", "o1", "i1");
  composite.createInternalConnection("neg1", "l2", "o1", "i2");
  composite.createInternalConnection("lt1", "const1", "o1", "i1");
  composite.createInternalConnection("const1", "l1", "o1", "i2");
  /*** second route ***/

  /*** third route ***/
  composite.createInternalConnection("diff2", "et1", "o1", "i1");
  composite.createInternalConnection("et1", "l1", "o1", "i2");
  composite.createInternalConnection("et1", "l2", "o1", "i2");
  /*** third route ***/

  /*** first solution flow ***/

  composite.createInternalConnection("etn2", "varg", "o1", "i1");
  composite.createInternalConnection("et", "varg", "o1", "c1");
  composite.createInternalConnection("etn2", "varl", "o1", "i1");
  composite.createInternalConnection("et", "varl", "o1", "c1");

  composite.createInternalConnection("varg", "divide", "o1", "i1");
  composite.createInternalConnection("varl", "divide", "o1", "i2");
  /*** first solution flow ***/

  /*** final route ***/
  composite.createInternalConnection("l1", "divide", "o1", "i1");
  composite.createInternalConnection("l2", "divide", "o1", "i2");
  composite.createInternalConnection("divide", "add1", "o1", "i1");
  composite.createInternalConnection("add1", "-power1", "o1", "i1");
  composite.createInternalConnection("-power1", "-scale100", "o1", "i1");
  composite.createInternalConnection("-scale100", "add100", "o1", "i1");
  composite.createInternalConnection("add100", "out", "o1", "i1");
  /*** final route ***/

  composite.connect(c1);

  std::vector<double> values = {54.8,  56.8,  57.85, 59.85, 60.57, 61.1,  62.17, 60.6,  62.35, 62.15,
                                62.35, 61.45, 62.8,  61.37, 62.5,  62.57, 60.8,  59.37, 60.35, 62.35,
                                62.17, 62.55, 64.55, 64.37, 65.3,  64.42, 62.9,  61.6,  62.05, 60.05,
                                59.7,  60.9,  60.25, 58.27, 58.7,  57.72, 58.1,  58.2,  0};

  std::vector<double> rsis = {0,        0,        0,        0,        0,        0,        0,        0,
                              0,        0,        0,        0,        0,        0,        74.21384, 74.33552,
                              65.87129, 59.93370, 62.43288, 66.96205, 66.18862, 67.05377, 71.22679, 70.36299,
                              72.23644, 67.86486, 60.99822, 55.79821, 57.15964, 49.81579, 48.63810, 52.76154,
                              50.40119, 43.95111, 45.57992, 42.54534, 44.09946, 44.52472, 7.71906};

  auto rsi = RelativeStrengthIndex<std::uint64_t, double>("rsi", 14);

  rsi.connect(c2);

  SECTION("Emit rsi") {
    for (int i = 0; i < values.size(); i++) {
      rsi.receiveData(Message<uint64_t, double>(i + 1, values.at(i)));
      composite.receiveData(Message<uint64_t, double>(i + 1, values.at(i)));
    }

    for (int i = 0; i < c1.getDataInputSize("i1"); i++) {
      REQUIRE(abs(c1.getDataInputMessage("i1", i).value - c2.getDataInputMessage("i1", i).value) < 0.00000001);
      REQUIRE(c1.getDataInputMessage("i1", i).time == c2.getDataInputMessage("i1", i).time);
    }
  }
}
