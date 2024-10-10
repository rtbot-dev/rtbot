#include <catch2/catch.hpp>
#include <iomanip>
#include <sstream>
#include <string>

std::string hexStr(const uint8_t *data, int len) {
  std::stringstream ss;
  ss << std::hex;

  for (int i(0); i < len; ++i) ss << std::setw(2) << std::setfill('0') << (int)data[i];

  return ss.str();
}

#include "rtbot/Join.h"
#include "rtbot/std/Variable.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Operator restore test") {
  auto join = Join<uint64_t, double>("join", 2);

  SECTION("Operator dataInputs can be collected and restored") {
    for (int i = 0; i < 5; i++) {
      join.receiveData(Message<uint64_t, double>(i * 10, i), "i1");
      join.receiveData(Message<uint64_t, double>(i * 10, i * i), "i2");
    }

    Bytes bytes = join.collect();
    // print the bytes in hex format
    // cout << hexStr(bytes.data(), bytes.size()) << endl;

    // create a second Join operator with the same constructor arguments
    auto join2 = Join<uint64_t, double>("join", 2);
    // restore the state in the second operator
    Bytes::const_iterator it = bytes.begin();
    join2.restore(it);

    // check that the dataInputs of the two operators are the same
    for (auto [port, data2] : join2.dataInputs) {
      auto data1 = join.dataInputs.at(port);
      for (uint i = 0; i < data2.size(); i++) {
        auto msg1 = data1.at(i);
        auto msg2 = data2.at(i);
        REQUIRE(msg1.time == msg2.time);
        REQUIRE(msg1.value == msg2.value);
      }
    }
  }
}
