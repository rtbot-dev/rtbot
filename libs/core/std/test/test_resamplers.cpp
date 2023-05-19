#include <catch2/catch.hpp>
#include <iostream>

#include "rtbot/std/CosineResampler.h"
#include "rtbot/std/HermiteResampler.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Cosine Resampler test emit at right frequencies") {
  auto i1 = CosineResampler<std::uint64_t, double>("i1", 99);
  auto i2 = CosineResampler<std::uint64_t, double>("i2", 99);

  SECTION("emits once") {
    for (int i = 0; i < 20; i++) {
      map<string, std::vector<Message<std::uint64_t, double>>> emitted =
          i1.receive(Message<std::uint64_t, double>(i * 100, i * i));
      if (i == 0)
        REQUIRE(emitted.empty());
      else {
        REQUIRE(emitted.find("i1")->second.at(0).time == 99 * i);
      }
    }
  }

  SECTION("emits twice") {
    for (int i = 0; i < 20; i++) {
      map<string, std::vector<Message<std::uint64_t, double>>> emitted =
          i2.receive(Message<std::uint64_t, double>(i * 200, i * i));
      if (i == 0)
        REQUIRE(emitted.empty());
      else {
        REQUIRE(emitted.find("i2")->second.at(0).time == 99 * (2 * i - 1));
        REQUIRE(emitted.find("i2")->second.at(1).time == 99 * (2 * i));
      }
    }
  }
}

TEST_CASE("Hermite Resampler test emit at right frequencies") {
  auto i1 = HermiteResampler<std::uint64_t, double>("i1", 99);
  auto i2 = HermiteResampler<std::uint64_t, double>("i2", 99);

  SECTION("emits once") {
    for (int i = 0; i < 20; i++) {
      map<string, std::vector<Message<std::uint64_t, double>>> emitted =
          i1.receive(Message<std::uint64_t, double>(i * 100, i * i));
      if (i == 0 || i == 1 || i == 2)
        REQUIRE(emitted.empty());
      else if (i == 3) {
        REQUIRE(emitted.find("i1")->second.at(0).time == 99 * (i - 2));
        REQUIRE(emitted.find("i1")->second.at(1).time == 99 * (i - 1));
      } else {
        REQUIRE(emitted.find("i1")->second.at(0).time == 99 * (i - 1));
      }
    }
  }

  SECTION("emits twice") {
    for (int i = 0; i < 20; i++) {
      map<string, std::vector<Message<std::uint64_t, double>>> emitted =
          i2.receive(Message<std::uint64_t, double>(i * 200, i * i));
      if (i == 0 || i == 1 || i == 2)
        REQUIRE(emitted.empty());
      else if (i == 3) {
        REQUIRE(emitted.find("i2")->second.at(0).time == 99 * (i - 2));
        REQUIRE(emitted.find("i2")->second.at(1).time == 99 * (i - 1));
        REQUIRE(emitted.find("i2")->second.at(2).time == 99 * i);
        REQUIRE(emitted.find("i2")->second.at(3).time == 99 * (i + 1));
      } else {
        REQUIRE(emitted.find("i2")->second.at(0).time == 99 * (2 * i - 3));
        REQUIRE(emitted.find("i2")->second.at(1).time == 99 * (2 * i - 2));
      }
    }
  }
}