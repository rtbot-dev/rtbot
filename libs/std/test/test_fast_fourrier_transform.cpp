#include <catch2/catch.hpp>

#include "rtbot/std/FastFourrierTransform.h"

using namespace rtbot;
using namespace std;

void printEmittedMessages(
    const std::map<std::string, std::map<std::string, std::vector<Message<uint64_t, double>>>>& emitted) {
  for (const auto& outerPair : emitted) {
    std::cout << outerPair.first << ": " << std::endl;
    for (const auto& innerPair : outerPair.second) {
      std::cout << "\t" << innerPair.first << ": ";
      for (const auto& msg : innerPair.second) {
        std::cout << "Time: " << msg.time << ", Value: " << msg.value << "; ";
      }
      std::cout << std::endl;
    }
  }
}

TEST_CASE("FastFourrierTransform") {
  SECTION("FFT computation and output messages") {
    auto fft = FastFourrierTransform<uint64_t, double>("fft", 3, 1, true, true, true);
    vector<double> signal = {1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0};
    // Create a sine wave input
    for (uint64_t i = 0; i < signal.size(); i++) {
      // double value = sin(2 * M_PI * i / 100.0);
      fft.receiveData(Message<uint64_t, double>(i, signal[i]));
    }

    // Process the data and get the output messages
    auto emitted = fft.executeData();
    // printEmittedMessages(emitted);

    vector<double> realExpected = {4, 1, 0, 1, 0, 1, 0, 1};
    vector<double> imagExpected = {0.0, -2.4142136, 0.0, -0.4142136, 0.0, 0.4142136, 0.0, 2.4142136};
    vector<double> powerExpected = {16, 6.8284271, 0, 1.171573, 0, 1.171573, 0, 6.8284271};
    for (size_t i = 0; i < fft.getSize(); i++) {
      REQUIRE(emitted.find("fft")->second.find("re" + to_string(i + 1))->second.at(0).value == Approx(realExpected[i]));
      REQUIRE(emitted.find("fft")->second.find("im" + to_string(i + 1))->second.at(0).value == Approx(imagExpected[i]));
      REQUIRE(emitted.find("fft")->second.find("p" + to_string(i + 1))->second.at(0).value == Approx(powerExpected[i]));
    }
  }
}
