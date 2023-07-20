#ifndef INPUT_H
#define INPUT_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Input : public Operator<T, V> {
  Input() = default;

  Input(string const &id, size_t numPorts = 1) : Operator<T, V>(id) {
    for (int i = 1; i <= numPorts; i++) {
      string inputPort = "i" + to_string(i);
      string outputPort = "o" + to_string(i);
      portsMap.emplace(inputPort, outputPort);
      this->addDataInput(inputPort, Input<T, V>::size);
      this->addOutput(outputPort);
    }
  }

  size_t getNumPorts() const { return this->dataInputs.size(); }

  string typeName() const override { return "Input"; }

  void receiveData(Message<T, V> msg, string inputPort = "") override {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (this->dataInputs.count(inputPort) > 0) {
      if (this->dataInputs.find(inputPort)->second.getMaxSize() == this->dataInputs.find(inputPort)->second.size()) {
        this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() -
                                                        this->dataInputs.find(inputPort)->second.front().value);
        this->dataInputs.find(inputPort)->second.pop_front();
      } else if (this->dataInputs.find(inputPort)->second.getMaxSize() <
                 this->dataInputs.find(inputPort)->second.size())
        throw std::runtime_error(typeName() + ": " + inputPort + " : went above maximum size");

      this->dataInputs.find(inputPort)->second.push_back(msg);
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() +
                                                      this->dataInputs.find(inputPort)->second.back().value);
      if (this->toProcess.count(inputPort) == 0) this->toProcess.insert(inputPort);
    } else
      throw std::runtime_error(typeName() + ": " + inputPort + " : refers to a non existing input port");
  }

  virtual map<string, map<string, vector<Message<T, V>>>> executeData() override {
    vector<string> toRemove;
    for (auto it = this->toProcess.begin(); it != this->toProcess.end(); ++it) {
      if (this->getDataInputMaxSize(*it) > this->getDataInputSize(*it)) toRemove.push_back(*it);
    }
    for (int i = 0; i < toRemove.size(); i++) this->toProcess.erase(toRemove.at(i));

    if (!this->toProcess.empty()) {
      auto toEmit = processData();
      if (!toEmit.empty()) return this->emit(toEmit);
    }
    return {};
  }

  virtual map<string, vector<Message<T, V>>> processData() override {
    map<string, vector<Message<T, V>>> outputMsgs;
    while (!this->toProcess.empty()) {
      string inputPort = *(this->toProcess.begin());
      Message<T, V> m1 = this->getDataInputMessage(inputPort, 1);
      Message<T, V> m0 = this->getDataInputMessage(inputPort, 0);
      if (m1.time > m0.time) {
        vector<Message<T, V>> v;
        v.push_back(m0);
        outputMsgs.emplace(portsMap.find(inputPort)->second, v);
      }
      this->toProcess.erase(inputPort);
    }
    return outputMsgs;
  }

 private:
  map<string, string> portsMap;
  static const int size = 2;
  set<string> toProcess;
};

}  // namespace rtbot

#endif  // INPUT_H
