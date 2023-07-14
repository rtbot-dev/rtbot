#include <catch2/catch.hpp>

#include "rtbot/std/CosineResampler.h"
#include "rtbot/std/HermiteResampler.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Cosine Resampler test emit at right frequencies") {
  auto i1 = CosineResampler<uint64_t, double>("i1", 99);
  auto i2 = CosineResampler<uint64_t, double>("i2", 99);

  SECTION("emits once") {
    for (int i = 0; i < 20; i++) {
      map<string, map<string, vector<Message<uint64_t, double>>>> emitted =
          i1.receiveData(Message<uint64_t, double>(i * 100, i * i));
      if (i == 0)
        REQUIRE(emitted.empty());
      else {
        REQUIRE(emitted.find("i1")->second.find("o1")->second.at(0).time == 99 * i);
      }
    }
  }

  SECTION("emits twice") {
    for (int i = 0; i < 20; i++) {
      map<string, map<string, vector<Message<uint64_t, double>>>> emitted =
          i2.receiveData(Message<uint64_t, double>(i * 200, i * i));
      if (i == 0)
        REQUIRE(emitted.empty());
      else {
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(0).time == 99 * (2 * i - 1));
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(1).time == 99 * (2 * i));
      }
    }
  }
}

TEST_CASE("Hermite Resampler test emit at right frequencies") {
  auto i1 = HermiteResampler<uint64_t, double>("i1", 99);
  auto i2 = HermiteResampler<uint64_t, double>("i2", 99);
  auto i3 = HermiteResampler<int64_t, double>("i3", 99);

  SECTION("emits once") {
    for (int i = 0; i < 20; i++) {
      map<string, map<string, vector<Message<uint64_t, double>>>> emitted =
          i1.receiveData(Message<uint64_t, double>(i * 100, i * i));
      if (i == 0 || i == 1 || i == 2)
        REQUIRE(emitted.empty());
      else if (i == 3) {
        REQUIRE(emitted.find("i1")->second.find("o1")->second.at(0).time == 99 * (i - 2));
        REQUIRE(emitted.find("i1")->second.find("o1")->second.at(1).time == 99 * (i - 1));
      } else {
        REQUIRE(emitted.find("i1")->second.find("o1")->second.at(0).time == 99 * (i - 1));
      }
    }
  }

  SECTION("emits twice") {
    for (int i = 0; i < 20; i++) {
      map<string, map<string, vector<Message<uint64_t, double>>>> emitted =
          i2.receiveData(Message<uint64_t, double>(i * 200, i * i));
      if (i == 0 || i == 1 || i == 2)
        REQUIRE(emitted.empty());
      else if (i == 3) {
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(0).time == 99 * (i - 2));
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(1).time == 99 * (i - 1));
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(2).time == 99 * i);
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(3).time == 99 * (i + 1));
      } else {
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(0).time == 99 * (2 * i - 3));
        REQUIRE(emitted.find("i2")->second.find("o1")->second.at(1).time == 99 * (2 * i - 2));
      }
    }
  }

  SECTION("emits right values") {
    for (int i = 0; i < 20; i++) {
      map<string, map<string, vector<Message<std::int64_t, double>>>> emitted =
          i3.receiveData(Message<std::int64_t, double>(i * 100, i * i));
      if (i == 0 || i == 1 || i == 2)
        REQUIRE(emitted.empty());
      else if (i == 3) {
        REQUIRE((emitted.find("i3")->second.find("o1")->second.at(0).value - (i - 2) * (i - 2)) < 1);
        REQUIRE((emitted.find("i3")->second.find("o1")->second.at(1).value - (i - 1) * (i - 1)) < 1);

      } else {
        REQUIRE((emitted.find("i3")->second.find("o1")->second.at(0).value - (i - 1) * (i - 1)) < 1);
      }
    }
  }
}