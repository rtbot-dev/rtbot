#include <catch2/catch.hpp>

#include "rtbot/Demultiplexer.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"
#include "rtbot/std/Constant.h"
#include "rtbot/std/Count.h"
#include "rtbot/std/EqualTo.h"
#include "rtbot/std/GreaterThan.h"
#include "rtbot/std/LessThan.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Demultiplexer") {
  auto in1 = Input<uint64_t, double>("in1");
  auto count = Count<uint64_t, double>("count");
  auto demult = Demultiplexer<uint64_t, double>("demult", 2);
  auto less = LessThan<uint64_t, double>("less", 20);
  auto greater = GreaterThan<uint64_t, double>("greater", 20);
  auto equal = EqualTo<uint64_t, double>("equal", 20);
  auto constLt20Zero = Constant<uint64_t, double>("cLt20Zero", 0);
  auto constLt20One = Constant<uint64_t, double>("cLt20One", 1);
  auto constEt20Zero = Constant<uint64_t, double>("cEt20Zero", 0);
  auto constEt20One = Constant<uint64_t, double>("cEt20One", 1);
  auto constGt20Zero = Constant<uint64_t, double>("cGt20Zero", 0);
  auto constGt20One = Constant<uint64_t, double>("cGt20One", 1);

  in1.connect(demult, "o1", "i1");
  in1.connect(count)->connect(less)->connect(constLt20One, "o1", "i1")->connect(demult, "o1", "c1");
  less.connect(constLt20Zero, "o1", "i1")->connect(demult, "o1", "c2");
  count.connect(equal)->connect(constEt20One, "o1", "i1")->connect(demult, "o1", "c2");
  equal.connect(constEt20Zero)->connect(demult, "o1", "c1");
  count.connect(greater)->connect(constGt20One, "o1", "i1")->connect(demult, "o1", "c2");
  greater.connect(constGt20Zero)->connect(demult, "o1", "c1");

  // process the data
  SECTION("Emit for the right port") {
    for (int i = 1; i <= 100; i++) {
      in1.receiveData(Message<uint64_t, double>(i, (i < 20) ? 1 : 2));
      auto output = in1.executeData();

      if (i < 20) {
        REQUIRE(output.find("demult")->second.find("o1")->second.size() == 1);
        REQUIRE(output.find("demult")->second.find("o1")->second.at(0).value == 1);
        REQUIRE(output.find("demult")->second.find("o1")->second.at(0).time == i);

        REQUIRE(output.find("demult")->second.count("o2") == 0);
      } else if (i >= 20) {
        REQUIRE(output.find("demult")->second.find("o2")->second.size() == 1);
        REQUIRE(output.find("demult")->second.find("o2")->second.at(0).value == 2);
        REQUIRE(output.find("demult")->second.find("o2")->second.at(0).time == i);

        REQUIRE(output.find("demult")->second.count("o1") == 0);
      }
    }
  }
}
