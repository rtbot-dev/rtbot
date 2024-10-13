#include <catch2/catch.hpp>

#include "rtbot/std/HermiteResampler.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Hermite Resampler test emit at right frequencies") {
  auto i1 = HermiteResampler<uint64_t, double>("i1", 99);
  auto i2 = HermiteResampler<uint64_t, double>("i2", 99);
  auto i3 = HermiteResampler<uint64_t, double>("i3", 99);
  auto i4 = HermiteResampler<uint64_t, double>("i4", 1);

  SECTION("emits once") {
    for (int i = 0; i < 20; i++) {
      i1.receiveData(Message<uint64_t, double>(i * 100, i * i));
      ProgramMessage<uint64_t, double> emitted = i1.executeData();

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
      i2.receiveData(Message<uint64_t, double>(i * 200, i * i));
      ProgramMessage<uint64_t, double> emitted = i2.executeData();

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
      i3.receiveData(Message<uint64_t, double>(i * 100, i * i));
      ProgramMessage<uint64_t, double> emitted = i3.executeData();

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

  SECTION("emits every time, one next to the next one") {
    srand(time(nullptr));
    vector<int> v;
    int start = 0;
    int end = 2000;
    int last = start;
    v.push_back(start);
    for (int i = start + 1; i < end; i++) {
      int random_value = rand();
      if (random_value < (RAND_MAX / 2)) {
        v.push_back(i);
      }
    }
    v.push_back(end);

    for (int i = 0; i < v.size(); i++) {
      i4.receiveData(Message<uint64_t, double>(v.at(i), v.at(i) * v.at(i)));
      ProgramMessage<uint64_t, double> emitted = i4.executeData();

      if (i == 0 || i == 1 || i == 2) {
        // cout << "******* (" << v.at(i) << ") *******" << endl;
        REQUIRE(emitted.empty());
      } else {
        // cout << "******* (" << v.at(i) << ") *******" << endl;
        for (int j = 0; j < emitted.find("i4")->second.find("o1")->second.size(); j++) {
          int now = emitted.find("i4")->second.find("o1")->second.at(j).time;
          REQUIRE(now - 1 == last);
          last = now;
          // cout << now << endl;
        }
      }
    }
  }
}