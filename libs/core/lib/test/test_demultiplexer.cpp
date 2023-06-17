#include <catch2/catch.hpp>

#include "rtbot/Demultiplexer.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/Count.h"
#include "rtbot/std/EqualTo.h"
#include "rtbot/std/GreaterThan.h"
#include "rtbot/std/LessThan.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Demultiplexer") {
  auto in1 = Input<uint64_t, double>("in1");
  auto count = Count<uint64_t, double>("count");
  auto demult = Demultiplexer<uint64_t, double>("demult");
  auto less = LessThan<uint64_t, double>("less", 20);
  auto greater = GreaterThan<uint64_t, double>("greater", 20);
  auto equal = EqualTo<uint64_t, double>("equal", 20);
  auto out1 = Output_vec<uint64_t, double>("out1", 1);
  auto out2 = Output_vec<uint64_t, double>("out2", 1);

  in1.connect(demult, "o1", "i1")->connect(out1, "o1", "i1");
  demult.connect(out2, "o2", "i1");
  in1.connect(count)->connect(less)->connect(demult, "o1", "c1");
  count.connect(equal)->connect(demult, "o1", "c2");
  count.connect(greater)->connect(demult, "o1", "c2");

  // process the data
  SECTION("Emit for the right port") {
    for (int i = 1; i <= 100; i++) {
      auto output = in1.receiveData(Message<uint64_t, double>(i, (i < 20) ? 1 : 2));

      if (i < 21 && i > 1) {
        REQUIRE(output["out1"].size() == 1);
        REQUIRE(output["out1"].at(0).value == 1);
        REQUIRE(output["out1"].at(0).time == (i - 1));

        REQUIRE(output["out2"].size() == 0);
      } else if (i >= 21) {
        REQUIRE(output["out2"].size() == 1);
        REQUIRE(output["out2"].at(0).value == 2);
        REQUIRE(output["out2"].at(0).time == (i - 1));

        REQUIRE(output["out1"].size() == 0);
      }
    }
  }
}
