#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/std/Count.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Count") {
  auto c = Count<uint64_t, double>("c");

  SECTION("emits count") {
    ProgramMessage<uint64_t, double> emitted;
    for (int i = 1; i <= 50; i++) {
      c.receiveData(Message<uint64_t, double>(i, i));
      emitted = c.executeData();
      REQUIRE(emitted.find("c")->second.find("o1")->second.at(0).value == i);
    }
  }

  SECTION("can be collected and restored") {
    auto c2 = Count<uint64_t, double>("c2");
    for (int i = 0; i < 50; i++) {
      c2.receiveData(Message<uint64_t, double>(i, i), "i1");
      // notice that we have to call this before collecting the state
      // if we want to observe any state change
      c2.executeData();
    }

    Bytes bytes = c2.collect();

    auto c3 = Count<uint64_t, double>("c3");
    Bytes::const_iterator it = bytes.begin();
    c3.restore(it);

    // now add a new message to both operators and check that the values
    // emitted by the two operators are the same

    c2.receiveData(Message<uint64_t, double>(50, 510), "i1");
    c3.receiveData(Message<uint64_t, double>(50, 510), "i1");

    ProgramMessage<uint64_t, double> emitted2 = c2.executeData();
    ProgramMessage<uint64_t, double> emitted3 = c3.executeData();

    REQUIRE(emitted2.find("c2")->second.find("o1")->second.at(0).value == 51);
    REQUIRE(emitted3.find("c3")->second.find("o1")->second.at(0).value == 51);
  }
}
