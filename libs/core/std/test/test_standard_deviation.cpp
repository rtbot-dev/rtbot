#include <catch2/catch.hpp>
#include <iostream>
#include <math.h>

#include "rtbot/std/StandardDeviation.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Standard deviation") {

  auto i1 = StandardDeviation("i1", 5);
  auto i2 = StandardDeviation("i2", 10);
  
  SECTION("emits zeros") {
    for (int i = 0; i < 20; i++) {
      map<string, std::vector<Message<>>> emitted = i1.receive(Message<>(i * 100, 10));      
      if (i <= 3)
      {        
        REQUIRE(emitted.empty());
      }
      else
        REQUIRE(emitted.find("i1")->second.at(0).value.at(0) == 0);
    }
  }

  SECTION("emits correct std") {
    for (int i = 0; i < 10; i++) {
      map<string, std::vector<Message<>>> emitted = i2.receive(Message<>(i * 100, i+1));      
      if (i <= 8)
      {        
        REQUIRE(emitted.empty());
      }
      else
        REQUIRE(((int)emitted.find("i2")->second.at(0).value.at(0)) == 3);
    }
  }

  
}