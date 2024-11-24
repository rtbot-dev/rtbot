#include <catch2/catch.hpp>
#include <iostream>
#include <sstream>

#include "rtbot/Input.h"
#include "rtbot/Multiplexer.h"
#include "rtbot/Output.h"
#include "rtbot/std/Constant.h"

using namespace rtbot;
using namespace std;

// Helper to capture stderr
class CaptureStream {
 public:
  CaptureStream(std::ostream& stream) : stream(stream) { old = stream.rdbuf(buffer.rdbuf()); }
  ~CaptureStream() { stream.rdbuf(old); }
  string get() const { return buffer.str(); }

 private:
  std::ostream& stream;
  std::stringstream buffer;
  std::streambuf* old;
};

TEST_CASE("Multiplexer") {
  SECTION("Basic routing functionality") {
    // Create a multiplexer with 2 inputs
    auto mult = Multiplexer<uint64_t, double>("mult", 2);
    auto zero = Constant<uint64_t, double>("zero", 0);
    auto one = Constant<uint64_t, double>("one", 1);

    // Create the control signals
    one.connect(mult, "o1", "c1");
    zero.connect(mult, "o1", "c2");

    // Test routing from first input
    mult.receiveData(Message<uint64_t, double>(1, 10.0), "i1");  // value on first input
    mult.receiveData(Message<uint64_t, double>(1, 20.0), "i2");  // value on second input

    // Send control signals to select first input
    one.receiveData(Message<uint64_t, double>(1, 1.0));   // control signal for i1
    zero.receiveData(Message<uint64_t, double>(1, 1.0));  // control signal for i2

    auto output = one.executeData();
    output = zero.executeData();

    // Should route value from first input
    REQUIRE(output.find("mult") != output.end());
    REQUIRE(output.find("mult")->second.find("o1") != output.find("mult")->second.end());
    REQUIRE(output.find("mult")->second.find("o1")->second.size() == 1);
    REQUIRE(output.find("mult")->second.find("o1")->second.at(0).value == 10.0);
    REQUIRE(output.find("mult")->second.find("o1")->second.at(0).time == 1);
  }
  SECTION("Switch inputs") {
    auto mult = Multiplexer<uint64_t, double>("mult", 2);
    auto zero = Constant<uint64_t, double>("zero", 0);
    auto one = Constant<uint64_t, double>("one", 1);

    one.connect(mult, "o1", "c2");   // Note: now selecting second input
    zero.connect(mult, "o1", "c1");  // First input disabled

    // Send data
    mult.receiveData(Message<uint64_t, double>(1, 10.0), "i1");
    mult.receiveData(Message<uint64_t, double>(1, 20.0), "i2");

    // Send control signals
    one.receiveData(Message<uint64_t, double>(1, 1.0));
    zero.receiveData(Message<uint64_t, double>(1, 1.0));

    auto output = one.executeData();
    output = zero.executeData();

    // Should route value from second input
    REQUIRE(output.find("mult") != output.end());
    REQUIRE(output.find("mult")->second.find("o1") != output.find("mult")->second.end());
    REQUIRE(output.find("mult")->second.find("o1")->second.size() == 1);
    REQUIRE(output.find("mult")->second.find("o1")->second.at(0).value == 20.0);
    REQUIRE(output.find("mult")->second.find("o1")->second.at(0).time == 1);
  }

  SECTION("Should not produce output when control signals are invalid") {
    CaptureStream capture(std::cerr);

    auto mult = Multiplexer<uint64_t, double>("mult", 2);
    auto one = Constant<uint64_t, double>("one", 1);

    one.connect(mult, "o1", "c1");
    one.connect(mult, "o1", "c2");

    // Send data
    mult.receiveData(Message<uint64_t, double>(1, 10.0), "i1");
    mult.receiveData(Message<uint64_t, double>(1, 20.0), "i2");

    // Invalid control signals (both 1)
    one.receiveData(Message<uint64_t, double>(1, 1.0));
    one.receiveData(Message<uint64_t, double>(1, 1.0));

    auto output = one.executeData();
    // Should not produce output when control signals are invalid
    REQUIRE(output.find("mult") == output.end());
  }

  SECTION("Warning when timestamps don't match") {
    CaptureStream capture(std::cerr);

    auto mult = Multiplexer<uint64_t, double>("mult", 2);
    auto zero = Constant<uint64_t, double>("zero", 0);
    auto one = Constant<uint64_t, double>("one", 1);

    one.connect(mult, "o1", "c1");
    zero.connect(mult, "o1", "c2");

    // Send data
    mult.receiveData(Message<uint64_t, double>(1, 10.0), "i1");
    mult.receiveData(Message<uint64_t, double>(1, 20.0), "i2");

    // Control signals with different timestamps
    one.receiveData(Message<uint64_t, double>(1, 1.0));
    zero.receiveData(Message<uint64_t, double>(2, 1.0));

    auto output = one.executeData();
    output = zero.executeData();

    // Should not produce output when timestamps don't match
    REQUIRE(output.find("mult") == output.end());
  }

  SECTION("Warning when no control signal is active") {
    CaptureStream capture(std::cerr);

    auto mult = Multiplexer<uint64_t, double>("mult", 2);
    auto zero = Constant<uint64_t, double>("zero", 0);

    zero.connect(mult, "o1", "c1");
    zero.connect(mult, "o1", "c2");

    // Send data
    mult.receiveData(Message<uint64_t, double>(1, 10.0), "i1");
    mult.receiveData(Message<uint64_t, double>(1, 20.0), "i2");

    // All control signals 0
    zero.receiveData(Message<uint64_t, double>(1, 1.0));
    zero.receiveData(Message<uint64_t, double>(1, 1.0));

    auto output = zero.executeData();

    // Should not produce output
    REQUIRE(output.find("mult") == output.end());

    // Should produce warning message
    string warning = capture.get();
    REQUIRE(warning.find("Warning: Multiplexer (mult): No control signal active") != string::npos);
  }

  SECTION("Warning with invalid control values") {
    CaptureStream capture(std::cerr);

    auto mult = Multiplexer<uint64_t, double>("mult", 2);
    auto invalid = Constant<uint64_t, double>("invalid", 2);  // Invalid value
    auto zero = Constant<uint64_t, double>("zero", 0);

    invalid.connect(mult, "o1", "c1");
    zero.connect(mult, "o1", "c2");

    // Send data
    mult.receiveData(Message<uint64_t, double>(1, 10.0), "i1");
    mult.receiveData(Message<uint64_t, double>(1, 20.0), "i2");

    // Invalid control value
    invalid.receiveData(Message<uint64_t, double>(1, 1.0));
    zero.receiveData(Message<uint64_t, double>(1, 1.0));

    auto output = invalid.executeData();
    output = zero.executeData();

    // Should not produce output
    REQUIRE(output.find("mult") == output.end());

    // Should produce warning message
    string warning = capture.get();
    REQUIRE(warning.find("Warning: Multiplexer (mult): Invalid control value") != string::npos);
  }

  SECTION("Irregular timestamp patterns") {
    auto mult = Multiplexer<uint64_t, double>("mult", 3);

    // Control messages with irregular timestamps
    // c1: early timestamps + big gap + late timestamps
    mult.receiveControl(Message<uint64_t, double>(1, 0), "c1");
    mult.receiveControl(Message<uint64_t, double>(2, 0), "c1");
    mult.receiveControl(Message<uint64_t, double>(10, 0), "c1");
    mult.receiveControl(Message<uint64_t, double>(20, 0), "c1");

    // c2: sparse timestamps with selected port
    mult.receiveControl(Message<uint64_t, double>(2, 0), "c2");
    mult.receiveControl(Message<uint64_t, double>(5, 1), "c2");
    mult.receiveControl(Message<uint64_t, double>(10, 1), "c2");
    mult.receiveControl(Message<uint64_t, double>(15, 1), "c2");

    // c3: dense timestamps in the middle
    mult.receiveControl(Message<uint64_t, double>(5, 0), "c3");
    mult.receiveControl(Message<uint64_t, double>(6, 0), "c3");
    mult.receiveControl(Message<uint64_t, double>(7, 0), "c3");
    mult.receiveControl(Message<uint64_t, double>(8, 0), "c3");
    mult.receiveControl(Message<uint64_t, double>(9, 0), "c3");
    mult.receiveControl(Message<uint64_t, double>(10, 0), "c3");
    mult.receiveControl(Message<uint64_t, double>(15, 0), "c3");

    // Data messages with corresponding irregular patterns
    // i1: sparse data
    mult.receiveData(Message<uint64_t, double>(1, 10.0), "i1");
    mult.receiveData(Message<uint64_t, double>(10, 11.0), "i1");
    mult.receiveData(Message<uint64_t, double>(20, 12.0), "i1");

    // i2: more frequent data (selected input)
    mult.receiveData(Message<uint64_t, double>(2, 20.0), "i2");
    mult.receiveData(Message<uint64_t, double>(5, 21.0), "i2");
    mult.receiveData(Message<uint64_t, double>(10, 22.0), "i2");
    mult.receiveData(Message<uint64_t, double>(15, 23.0), "i2");

    // i3: dense data in middle range
    mult.receiveData(Message<uint64_t, double>(5, 30.0), "i3");
    mult.receiveData(Message<uint64_t, double>(6, 31.0), "i3");
    mult.receiveData(Message<uint64_t, double>(7, 32.0), "i3");
    mult.receiveData(Message<uint64_t, double>(8, 33.0), "i3");
    mult.receiveData(Message<uint64_t, double>(9, 34.0), "i3");
    mult.receiveData(Message<uint64_t, double>(10, 35.0), "i3");
    mult.receiveData(Message<uint64_t, double>(15, 36.0), "i3");

    auto output = mult.processData();

    // At t=10
    REQUIRE(output.find("o1") != output.end());
    REQUIRE(output.find("o1")->second.at(0).time == 10);
    REQUIRE(output.find("o1")->second.at(0).value == 22.0);
  }

  SECTION("Should emit multiple messages in sequence") {
    auto mult = Multiplexer<uint64_t, double>("mult", 2);
    auto zero = Constant<uint64_t, double>("zero", 0);
    auto one = Constant<uint64_t, double>("one", 1);

    // Connect control signals
    one.connect(mult, "o1", "c1");
    zero.connect(mult, "o1", "c2");

    cout << "Should emit multiple messages in sequence" << endl;
    cout << "----------------------------------------" << endl;
    // Send multiple data messages to both inputs
    mult.receiveData(Message<uint64_t, double>(1, 10.0), "i1");
    mult.receiveData(Message<uint64_t, double>(2, 11.0), "i1");
    mult.receiveData(Message<uint64_t, double>(3, 12.0), "i1");

    mult.receiveData(Message<uint64_t, double>(1, 20.0), "i2");
    mult.receiveData(Message<uint64_t, double>(2, 21.0), "i2");
    mult.receiveData(Message<uint64_t, double>(3, 22.0), "i2");

    // Send control signals for all timestamps
    for (uint64_t t = 1; t <= 3; t++) {
      cout << "Send one control signal for timestamp " << t << endl;
      one.receiveData(Message<uint64_t, double>(t, 1.0));  // Select first input
      cout << "Send zero control signal for timestamp " << t << endl;
      zero.receiveData(Message<uint64_t, double>(t, 1.0));  // Disable second input
    }

    // Process all messages
    auto output = one.executeData();
    output = zero.executeData();

    // print output pretty
    for (const auto& pair : output) {
      cout << pair.first << ": ";
      for (const auto& msg : pair.second.find("o1")->second) {
        cout << "(" << msg.time << ", " << msg.value << ") ";
      }
      cout << endl;
    }

    // Verify output contains all messages from the selected input
    REQUIRE(output.find("mult") != output.end());
    REQUIRE(output.find("mult")->second.find("o1")->second.size() == 3);

    // Check each message
    auto& messages = output.find("mult")->second.find("o1")->second;
    REQUIRE(messages[0].time == 1);
    REQUIRE(messages[0].value == 10.0);
    REQUIRE(messages[1].time == 2);
    REQUIRE(messages[1].value == 11.0);
    REQUIRE(messages[2].time == 3);
    REQUIRE(messages[2].value == 12.0);
  }
  * /
}