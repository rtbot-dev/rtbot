#include <catch2/catch.hpp>

#include "rtbot/std/FastFourrierTransform.h"

using namespace rtbot;
using namespace std;

TEST_CASE("FastFourrierTransform") {
  SECTION("FFT computation and output messages") {
    auto fft = FastFourrierTransform<uint64_t, double>("fft", 3, 1);
    cout << "fft.n = " << 3 << endl;
    // Create a sine wave input
    for (uint64_t i = 0; i < 100; i++) {
      double value = sin(2 * M_PI * i / 100.0);
      fft.receiveData(Message<uint64_t, double>(i, value));
    }

    // Process the data and get the output messages
    auto emitted = fft.executeData();
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
    // Check if the output messages match the expected output
    // The expected output is not known in advance, so we just check if the output is valid
    REQUIRE(emitted.find("fft")->second.find("w4")->second.at(0).value == Approx(-1.0));
  }
}
