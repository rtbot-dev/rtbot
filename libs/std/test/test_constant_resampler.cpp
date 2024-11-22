#include <catch2/catch.hpp>

#include "rtbot/std/ConstantResampler.h"

using namespace rtbot;
using namespace std;

TEST_CASE("ConstantResampler") {
  SECTION("Constructor validation") {
    REQUIRE_NOTHROW(ConstantResampler<uint64_t, double>("test", 5));
    REQUIRE_THROWS(ConstantResampler<uint64_t, double>("test", 0));
  }

  SECTION("Basic resampling with causal consistency") {
    auto resampler = ConstantResampler<uint64_t, double>("test", 3);

    // First message - should not emit
    resampler.receiveData(Message<uint64_t, double>(1, 10.0));
    auto result1 = resampler.executeData();
    REQUIRE(result1.empty());
    REQUIRE(resampler.getNextEmissionTime() == 4);
    REQUIRE(resampler.getLastValue() == 10.0);

    // Message at emission time - should emit with previous value
    resampler.receiveData(Message<uint64_t, double>(5, 50.0));
    auto result2 = resampler.executeData();
    REQUIRE(!result2.empty());
    auto& emissions = result2.find("test")->second.find("o1")->second;
    REQUIRE(emissions[0].time == 4);
    REQUIRE(emissions[0].value == 10.0);        // Should use previous value
    REQUIRE(resampler.getLastValue() == 50.0);  // But update last value
  }

  SECTION("Multiple grid points between messages") {
    auto resampler = ConstantResampler<uint64_t, double>("test", 2);

    // Initialize
    resampler.receiveData(Message<uint64_t, double>(1, 10.0));
    auto result1 = resampler.executeData();
    REQUIRE(result1.empty());
    REQUIRE(resampler.getNextEmissionTime() == 3);

    // Send message with large gap - should emit at all grid points
    resampler.receiveData(Message<uint64_t, double>(8, 80.0));
    auto result2 = resampler.executeData();
    REQUIRE(!result2.empty());

    auto& emissions = result2.find("test")->second.find("o1")->second;
    REQUIRE(emissions.size() == 3);  // Should emit at t=3,5,7

    // All emissions should use the previous value (10.0)
    REQUIRE(emissions[0].time == 3);
    REQUIRE(emissions[0].value == 10.0);
    REQUIRE(emissions[1].time == 5);
    REQUIRE(emissions[1].value == 10.0);
    REQUIRE(emissions[2].time == 7);
    REQUIRE(emissions[2].value == 10.0);

    // Last value should be updated
    REQUIRE(resampler.getLastValue() == 80.0);
    REQUIRE(resampler.getNextEmissionTime() == 9);
  }

  SECTION("State serialization with lastValue") {
    auto resampler1 = ConstantResampler<uint64_t, double>("test", 3);

    // Initialize and process some data
    resampler1.receiveData(Message<uint64_t, double>(1, 10.0));
    resampler1.executeData();

    // Serialize state
    Bytes state = resampler1.collect();

    // Create new resampler and restore state
    auto resampler2 = ConstantResampler<uint64_t, double>("test", 3);
    Bytes::const_iterator it = state.begin();
    resampler2.restore(it);

    // Verify both resamplers have same state including lastValue
    REQUIRE(resampler1.getInterval() == resampler2.getInterval());
    REQUIRE(resampler1.getNextEmissionTime() == resampler2.getNextEmissionTime());
    REQUIRE(resampler1.isInitiated() == resampler2.isInitiated());
    REQUIRE(resampler1.getLastValue() == resampler2.getLastValue());
  }

  SECTION("Rapid message sequence") {
    auto resampler = ConstantResampler<uint64_t, double>("test", 5);

    resampler.receiveData(Message<uint64_t, double>(1, 10.0));
    auto result1 = resampler.executeData();
    REQUIRE(result1.empty());

    // Rapid sequence of messages before next emit time
    resampler.receiveData(Message<uint64_t, double>(2, 20.0));
    resampler.receiveData(Message<uint64_t, double>(3, 30.0));
    resampler.receiveData(Message<uint64_t, double>(4, 40.0));
    auto result2 = resampler.executeData();
    REQUIRE(result2.empty());

    // Message at emit time should use last value before emit time
    resampler.receiveData(Message<uint64_t, double>(7, 70.0));
    auto result3 = resampler.executeData();
    REQUIRE(!result3.empty());
    auto& emissions = result3.find("test")->second.find("o1")->second;
    REQUIRE(emissions[0].time == 6);
    REQUIRE(emissions[0].value == 40.0);  // Should use value from t=4
  }

  SECTION("Grid-aligned messages") {
    auto resampler = ConstantResampler<uint64_t, double>("test", 5);

    // Initialize
    resampler.receiveData(Message<uint64_t, double>(1, 10.0));
    auto result1 = resampler.executeData();
    REQUIRE(result1.empty());
    REQUIRE(resampler.getNextEmissionTime() == 6);

    // Send messages before grid point
    resampler.receiveData(Message<uint64_t, double>(3, 30.0));
    resampler.receiveData(Message<uint64_t, double>(4, 40.0));
    auto result2 = resampler.executeData();
    REQUIRE(result2.empty());

    // Send message exactly at grid point
    resampler.receiveData(Message<uint64_t, double>(6, 60.0));
    auto result3 = resampler.executeData();
    REQUIRE(!result3.empty());
    auto& emissions = result3.find("test")->second.find("o1")->second;
    REQUIRE(emissions[0].time == 6);
    REQUIRE(emissions[0].value == 60.0);  // Should use current value, not previous
    REQUIRE(resampler.getNextEmissionTime() == 11);

    // Send message past grid point
    resampler.receiveData(Message<uint64_t, double>(13, 130.0));
    auto result4 = resampler.executeData();
    REQUIRE(!result4.empty());
    auto& emissions2 = result4.find("test")->second.find("o1")->second;
    REQUIRE(emissions2.size() == 1);
    REQUIRE(emissions2[0].time == 11);
    REQUIRE(emissions2[0].value == 60.0);  // Should use previous value
  }
}