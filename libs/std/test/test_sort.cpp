#include <catch2/catch.hpp>

#include "rtbot/std/Sort.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Sort") {
  SECTION("Sort ascending") {
    // Create a Sort operator with 3 input and 1 output
    Sort<uint64_t, double> sort("sort", 3, 1);

    // Create some unsorted input data
    vector<Message<uint64_t, double>> inputData1 = {{1, 5}, {2, 3}, {3, 11}, {4, 6}, {5, 12}};
    vector<Message<uint64_t, double>> inputData2 = {{1, 2}, {2, 4}, {3, 12}, {4, 8}, {5, 2}};
    vector<Message<uint64_t, double>> inputData3 = {{1, 3}, {2, 5}, {3, 13}, {4, 4}, {5, 32}};
    vector<Message<uint64_t, double>> expectedOutput1 = {{1, 2}, {2, 3}, {3, 11}, {4, 4}, {5, 2}};

    // Feed the input data to the Sort operator
    for (size_t i = 0; i < inputData1.size(); i++) {
      sort.receiveData(inputData1[i], "i1");
      sort.receiveData(inputData2[i], "i2");
      sort.receiveData(inputData3[i], "i3");
      auto outputMsgs = sort.executeData();
      // Check that the output is sorted
      REQUIRE(outputMsgs.find("sort")->second.find("o1")->second.at(0) == expectedOutput1[i]);
    }
  }

  SECTION("Sort descending") {
    // Create a Sort operator with 3 input and 1 output
    Sort<uint64_t, double> sort("sort", 3, 1, false);

    // Create some unsorted input data
    vector<Message<uint64_t, double>> inputData1 = {{1, 5}, {2, 3}, {3, 11}, {4, 6}, {5, 12}};
    vector<Message<uint64_t, double>> inputData2 = {{1, 2}, {2, 4}, {3, 12}, {4, 8}, {5, 2}};
    vector<Message<uint64_t, double>> inputData3 = {{1, 3}, {2, 5}, {3, 13}, {4, 4}, {5, 32}};
    vector<Message<uint64_t, double>> expectedOutput1 = {{1, 5}, {2, 5}, {3, 13}, {4, 8}, {5, 32}};

    // Feed the input data to the Sort operator
    for (size_t i = 0; i < inputData1.size(); i++) {
      sort.receiveData(inputData1[i], "i1");
      sort.receiveData(inputData2[i], "i2");
      sort.receiveData(inputData3[i], "i3");
      auto outputMsgs = sort.executeData();
      // Check that the output is sorted
      REQUIRE(outputMsgs.find("sort")->second.find("o1")->second.at(0) == expectedOutput1[i]);
    }
  }

  SECTION("Sort with more than 2 outputs") {
    // Create a Sort operator with 3 input and 2 output
    Sort<uint64_t, double> sort("sort", 3, 2);

    // Create some unsorted input data
    vector<Message<uint64_t, double>> inputData1 = {{1, 5}, {2, 3}, {3, 11}, {4, 6}, {5, 12}};
    vector<Message<uint64_t, double>> inputData2 = {{1, 2}, {2, 4}, {3, 12}, {4, 8}, {5, 2}};
    vector<Message<uint64_t, double>> inputData3 = {{1, 3}, {2, 5}, {3, 13}, {4, 4}, {5, 32}};
    vector<Message<uint64_t, double>> expectedOutput1 = {{1, 2}, {2, 3}, {3, 11}, {4, 4}, {5, 2}};
    vector<Message<uint64_t, double>> expectedOutput2 = {{1, 3}, {2, 4}, {3, 12}, {4, 6}, {5, 12}};

    // Feed the input data to the Sort operator
    for (size_t i = 0; i < inputData1.size(); i++) {
      sort.receiveData(inputData1[i], "i1");
      sort.receiveData(inputData2[i], "i2");
      sort.receiveData(inputData3[i], "i3");
      auto outputMsgs = sort.executeData();
      // Check that the output is sorted
      REQUIRE(outputMsgs.find("sort")->second.find("o1")->second.at(0) == expectedOutput1[i]);
      REQUIRE(outputMsgs.find("sort")->second.find("o2")->second.at(0) == expectedOutput2[i]);
    }
  }
}