#include <catch2/catch.hpp>
#include <cmath>

#include "rtbot/std/Function.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Function operator - Linear interpolation") {
  vector<pair<double, double>> points = {{0.0, 0.0}, {1.0, 2.0}, {2.0, 4.0}, {3.0, 6.0}};
  auto func = Function<uint64_t, double>("func", points, "linear");

  SECTION("Interpolation between points") {
    func.receiveData(Message<uint64_t, double>(1, 0.5));
    auto output = func.executeData();
    REQUIRE(output.find("func")->second.find("o1")->second.at(0).value == Approx(1.0));

    func.receiveData(Message<uint64_t, double>(2, 1.5));
    output = func.executeData();
    REQUIRE(output.find("func")->second.find("o1")->second.at(0).value == Approx(3.0));
  }

  SECTION("Extrapolation before first point") {
    func.receiveData(Message<uint64_t, double>(1, -1.0));
    auto output = func.executeData();
    REQUIRE(output.find("func")->second.find("o1")->second.at(0).value == Approx(-2.0));
  }

  SECTION("Extrapolation after last point") {
    func.receiveData(Message<uint64_t, double>(1, 4.0));
    auto output = func.executeData();
    REQUIRE(output.find("func")->second.find("o1")->second.at(0).value == Approx(8.0));
  }
}

TEST_CASE("Function operator - Hermite interpolation") {
  vector<pair<double, double>> points = {{0.0, 0.0}, {1.0, 1.0}, {2.0, 0.0}, {3.0, 1.0}};
  auto func = Function<uint64_t, double>("func", points, "hermite");

  SECTION("Interpolation between points") {
    func.receiveData(Message<uint64_t, double>(1, 0.5));
    auto output = func.executeData();
    REQUIRE(output.find("func")->second.find("o1")->second.at(0).value > 0.5);

    func.receiveData(Message<uint64_t, double>(2, 1.5));
    output = func.executeData();
    double y = output.find("func")->second.find("o1")->second.at(0).value;
    REQUIRE(y >= 0.0);
    REQUIRE(y <= 1.0);
  }
}

TEST_CASE("Function operator - Serialization") {
  vector<pair<double, double>> points = {{0.0, 0.0}, {1.0, 2.0}, {2.0, 4.0}, {3.0, 6.0}};
  auto func1 = Function<uint64_t, double>("func", points, "linear");

  // Add some data and process it
  func1.receiveData(Message<uint64_t, double>(1, 1.5));
  auto output1 = func1.executeData();

  // Serialize
  Bytes bytes = func1.collect();

  // Create new operator and restore state
  auto func2 = Function<uint64_t, double>("func", {{0.0, 0.0}, {1.0, 1.0}}, "linear");  // Different initial state
  Bytes::const_iterator it = bytes.begin();
  func2.restore(it);

  // Verify state was properly restored
  REQUIRE(func2.getPoints() == points);
  REQUIRE(func2.getInterpolationType() == "linear");

  // Verify behavior is identical
  func2.receiveData(Message<uint64_t, double>(1, 1.5));
  auto output2 = func2.executeData();
  REQUIRE(output2.find("func")->second.find("o1")->second.at(0).value ==
          output1.find("func")->second.find("o1")->second.at(0).value);
}