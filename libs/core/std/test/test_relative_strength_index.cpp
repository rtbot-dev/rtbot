#include <math.h>

#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/std/RelativeStrengthIndex.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Relative Strength Index") {
  auto rsi = RelativeStrengthIndex<std::uint64_t, double>("rsi", 15);
  std::vector<double> values = {54.8,  56.8,  57.85, 59.85, 60.57, 61.1,  62.17, 60.6,  62.35, 62.15,
                                62.35, 61.45, 62.8,  61.37, 62.5,  62.57, 60.8,  59.37, 60.35, 62.35,
                                62.17, 62.55, 64.55, 64.37, 65.3,  64.42, 62.9,  61.6,  62.05, 60.05,
                                59.7,  60.9,  60.25, 58.27, 58.7,  57.72, 58.1,  58.2};

  std::vector<double> rsis = {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
                              0,     74.36, 74.55, 65.75, 59.68, 61.98, 66.44, 65.75, 67,    70.76, 69.79, 71.43, 67.32,
                              60.78, 55.56, 56.71, 49.49, 48.19, 52.38, 50,    43.5,  45.36, 42.53, 44.13, 44.13};

  SECTION("emits right values") {
    for (int i = 0; i < values.size(); i++) {
      map<string, std::vector<Message<std::uint64_t, double>>> emitted =
          rsi.receive(Message<std::uint64_t, double>(i + 1, values.at(i)));
      if (i < 14) {
        REQUIRE(emitted.empty());
      } else {
        REQUIRE(abs(emitted.find("rsi")->second.at(0).value - rsis.at(i)) <= 1);
      }
    }
  }
}