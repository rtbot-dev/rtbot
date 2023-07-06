#define CATCH_CONFIG_MAIN
#include <math.h>

#include <catch2/catch.hpp>

#include "rtbot/finance/RelativeStrengthIndex.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Relative Strength Index") {
  auto rsi = RelativeStrengthIndex<std::uint64_t, double>("rsi", 14);
  std::vector<double> values = {54.8,  56.8,  57.85, 59.85, 60.57, 61.1,  62.17, 60.6,  62.35, 62.15,
                                62.35, 61.45, 62.8,  61.37, 62.5,  62.57, 60.8,  59.37, 60.35, 62.35,
                                62.17, 62.55, 64.55, 64.37, 65.3,  64.42, 62.9,  61.6,  62.05, 60.05,
                                59.7,  60.9,  60.25, 58.27, 58.7,  57.72, 58.1,  58.2};

  std::vector<double> rsis = {0,        0,        0,        0,        0,        0,        0,        0,
                              0,        0,        0,        0,        0,        0,        74.21384, 74.33552,
                              65.87129, 59.93370, 62.43288, 66.96205, 66.18862, 67.05377, 71.22679, 70.36299,
                              72.23644, 67.86486, 60.99822, 55.79821, 57.15964, 49.81579, 48.63810, 52.76154,
                              50.40119, 43.95111, 45.57992, 42.54534, 44.09946, 44.52472};

  SECTION("emits right values") {
    for (int i = 0; i < values.size(); i++) {
      map<string, std::vector<Message<std::uint64_t, double>>> emitted =
          rsi.receiveData(Message<std::uint64_t, double>(i + 1, values.at(i)));
      if (i < 14) {
        REQUIRE(emitted.empty());
      } else {
        REQUIRE(abs(emitted.find("rsi")->second.at(0).value - rsis.at(i)) <= 0.00001);
      }
    }
  }
}