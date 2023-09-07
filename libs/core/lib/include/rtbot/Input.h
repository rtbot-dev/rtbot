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
      this->addDataInput(inputPort, 2);
      this->addOutput(outputPort);
    }
  }

  size_t getNumPorts() const { return this->dataInputs.size(); }

  string typeName() const override { return "Input"; }

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
};

}  // namespace rtbot

#endif  // INPUT_H
