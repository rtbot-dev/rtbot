#ifndef POWER_H
#define POWER_H

#include <cmath>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Power : public Operator<T, V> {
  Power() = default;
  Power(string const &id, V value) : Operator<T, V>(id) {
    this->addDataInput("i1", 1);
    this->addOutput("o1");
    this->value = value;
  }
  string typeName() const override { return "Power"; }
  PortPayload<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    PortPayload<T, V> outputMsgs;
    Messages<T, V> toEmit;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    out.value = pow(out.value, this->value);
    toEmit.push_back(out);
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }

  V getValue() const { return this->value; }

 private:
  V value;
};

}  // namespace rtbot

#endif  // POWER_H
