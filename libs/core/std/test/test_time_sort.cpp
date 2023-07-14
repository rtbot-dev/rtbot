#include <catch2/catch.hpp>

#include "rtbot/Output.h"
#include "rtbot/std/TimeSort.h"

using namespace rtbot;
using namespace std;

TEST_CASE("TimeSort test increasing") {
  map<string, map<string, vector<Message<uint64_t, double>>>> emitted;
  auto ts = TimeSort<uint64_t, double>("ts", 2, true);
  auto out1 = Output_vec<uint64_t, double>("out1");
  auto out2 = Output_vec<uint64_t, double>("out2");
  ts.connect(out1, "o1", "i1");
  ts.connect(out2, "o2", "i1");

  ts.receiveData(Message<uint64_t, double>(2, 2), "i1");
  emitted = ts.receiveData(Message<uint64_t, double>(1, 1), "i2");

  REQUIRE(emitted.find("out1")->second.find("o1")->second.at(0).value == 1);
  REQUIRE(emitted.find("out1")->second.find("o1")->second.at(0).time == 1);

  REQUIRE(emitted.find("out2")->second.find("o1")->second.at(0).value == 2);
  REQUIRE(emitted.find("out2")->second.find("o1")->second.at(0).time == 2);
}

TEST_CASE("TimeSort test decreasing") {
  map<string, map<string, vector<Message<uint64_t, double>>>> emitted;
  auto ts = TimeSort<uint64_t, double>("ts", 2, false);
  auto out1 = Output_vec<uint64_t, double>("out1");
  auto out2 = Output_vec<uint64_t, double>("out2");
  ts.connect(out1, "o1", "i1");
  ts.connect(out2, "o2", "i1");

  ts.receiveData(Message<uint64_t, double>(2, 2), "i1");
  emitted = ts.receiveData(Message<uint64_t, double>(1, 1), "i2");

  REQUIRE(emitted.find("out1")->second.find("o1")->second.at(0).value == 2);
  REQUIRE(emitted.find("out1")->second.find("o1")->second.at(0).time == 2);

  REQUIRE(emitted.find("out2")->second.find("o1")->second.at(0).value == 1);
  REQUIRE(emitted.find("out2")->second.find("o1")->second.at(0).time == 1);
}
