#ifndef IDENTITY_H
#define IDENTITY_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Identity : public Operator<T, V> {
  Identity() = default;

  Identity(string const &id) : Operator<T, V>(id) {
    this->addDataInput("i1", 1);
    this->addOutput("o1");
  }

  string typeName() const override { return "Identity"; }

  OperatorMessage<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    OperatorMessage<T, V> outputMsgs;
    Message<T, V> out = this->getDataInputFirstMessage(inputPort);
    PortMessage<T, V> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }
};

}  // namespace rtbot

#endif  // IDENTITY_H
