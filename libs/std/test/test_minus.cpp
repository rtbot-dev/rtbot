#include <catch2/catch.hpp>

#include "rtbot/std/Minus.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Minus join") {
  ProgramMessage<uint64_t, double> emitted;
  auto minus = Minus<uint64_t, double>("minus");

  minus.receiveData(Message<uint64_t, double>(1, 1), "i1");
  minus.receiveData(Message<uint64_t, double>(2, 2), "i1");
  minus.receiveData(Message<uint64_t, double>(3, 3), "i1");
  minus.receiveData(Message<uint64_t, double>(4, 4), "i1");

  minus.receiveData(Message<uint64_t, double>(2, 3), "i2");
  emitted = minus.executeData();

  REQUIRE(emitted.find("minus")->second.find("o1")->second.at(0).value == -1);

  minus.receiveData(Message<uint64_t, double>(4, 4), "i2");
  emitted = minus.executeData();

  REQUIRE(emitted.find("minus")->second.find("o1")->second.at(0).value == 0);

  minus.receiveData(Message<uint64_t, double>(3, 5), "i2");
  emitted = minus.executeData();

  REQUIRE(emitted.empty());
}
