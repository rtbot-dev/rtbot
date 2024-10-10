#include <catch2/catch.hpp>

#include "rtbot/std/Variable.h"

using namespace rtbot;
using namespace std;

TEST_CASE("Variable sync") {
  ProgramMessage<uint64_t, double> emitted;
  auto variable = Variable<uint64_t, double>("variable", 0.5);

  variable.receiveData(Message<uint64_t, double>(1, 1.1), "i1");
  variable.receiveData(Message<uint64_t, double>(3, 1.2), "i1");
  variable.receiveData(Message<uint64_t, double>(5, 2.5), "i1");
  variable.receiveData(Message<uint64_t, double>(7, 2.8), "i1");
  variable.receiveData(Message<uint64_t, double>(10, 3.1), "i1");

  variable.receiveControl(Message<uint64_t, double>(0, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 0.5);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 0);

  variable.receiveControl(Message<uint64_t, double>(1, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 1.1);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 1);

  variable.receiveControl(Message<uint64_t, double>(2, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 1.1);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 2);

  variable.receiveControl(Message<uint64_t, double>(4, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 1.2);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 4);

  variable.receiveControl(Message<uint64_t, double>(5, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 2.5);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 5);

  variable.receiveControl(Message<uint64_t, double>(6, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 2.5);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 6);

  variable.receiveControl(Message<uint64_t, double>(7, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 2.8);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 7);

  variable.receiveControl(Message<uint64_t, double>(9, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 2.8);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 9);

  variable.receiveControl(Message<uint64_t, double>(10, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 3.1);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 10);

  variable.receiveControl(Message<uint64_t, double>(11, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(emitted.empty());

  variable.receiveData(Message<uint64_t, double>(12, 5.1), "i1");

  variable.receiveControl(Message<uint64_t, double>(11, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 3.1);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 11);

  variable.receiveControl(Message<uint64_t, double>(12, 0), "c1");
  emitted = variable.executeControl();

  REQUIRE(!emitted.empty());
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).value == 5.1);
  REQUIRE(emitted.find("variable")->second.find("o1")->second.at(0).time == 12);

  SECTION("Variable controInputs can be collected and restored") {
    auto var = Variable<uint64_t, double>("var", 1);
    for (int i = 0; i < 5; i++) {
      var.receiveData(Message<uint64_t, double>(i * 10, i), "i1");
      var.receiveControl(Message<unsigned long long, double>(i * 10, i), "c1");
    }

    Bytes bytes = var.collect();
    // print the bytes in hex format
    // cout << hexStr(bytes.data(), bytes.size()) << endl;

    // create a second Join operator with the same constructor arguments
    auto var2 = Variable<uint64_t, double>("var2", 2);
    // restore the state in the second operator
    Bytes::const_iterator it = bytes.begin();
    var2.restore(it);

    // check that the controlInputs of the two operators are the same
    for (auto [port, data2] : var2.controlInputs) {
      auto data1 = var.controlInputs.at(port);
      for (uint i = 0; i < data2.size(); i++) {
        auto msg1 = data1.at(i);
        auto msg2 = data2.at(i);
        REQUIRE(msg1.time == msg2.time);
        REQUIRE(msg1.value == msg2.value);
      }
    }

    // check that the dataInputs of the two operators are the same
    for (auto [port, data2] : var2.dataInputs) {
      auto data1 = var.dataInputs.at(port);
      for (uint i = 0; i < data2.size(); i++) {
        auto msg1 = data1.at(i);
        auto msg2 = data2.at(i);
        REQUIRE(msg1.time == msg2.time);
        REQUIRE(msg1.value == msg2.value);
      }
    }

    // check that the value of the two operators are the same
    REQUIRE(var2.getValue() == var.getValue());
  }
}
