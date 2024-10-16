#ifndef AUTOREGRESSIVE_H
#define AUTOREGRESSIVE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct AutoRegressive : public Operator<T, V> {
  vector<V> coeff;

  AutoRegressive() = default;

  AutoRegressive(string const& id, vector<V> const& coeff) : Operator<T, V>(id) {
    this->coeff = coeff;
    this->addDataInput("i1", this->coeff.size());
    this->addOutput("o1");
  }

  string typeName() const override { return "AutoRegressive"; }

  virtual void receiveData(Message<T, V> msg, string inputPort = "") override {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (this->dataInputs.count(inputPort) > 0) {
      size_t n = this->getDataInputMaxSize(inputPort);
      while (this->getDataInputSize(inputPort) < n)
        this->dataInputs.find(inputPort)->second.push_back(Message<T, V>(0, 0));  // boundary conditions=0
      // n + 1 added, so that processData can get the last one as a recipient
      this->dataInputs.find(inputPort)->second.push_back(msg);
    } else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
  }

  virtual ProgramMessage<T, V> executeData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    auto toEmit = processData();
    this->dataInputs.find(inputPort)->second.pop_front();
    this->dataInputs.find(inputPort)->second.pop_back();
    this->dataInputs.find(inputPort)->second.push_back(toEmit.find("o1")->second.at(0));
    return this->emit(toEmit);
  }

  virtual OperatorMessage<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    size_t n = this->getDataInputMaxSize(inputPort);
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    for (auto i = 0; i < n; i++) out.value += this->coeff[i] * this->getDataInputMessage(inputPort, i).value;
    OperatorMessage<T, V> toEmit;
    PortMessage<T, V> v;
    v.push_back(out);
    toEmit.emplace("o1", v);
    return toEmit;
  }
};

}  // namespace rtbot

#endif  // AUTOREGRESSIVE_H
