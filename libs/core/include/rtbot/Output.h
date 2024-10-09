#ifndef OUTPUT_H
#define OUTPUT_H

#include <functional>
#include <iostream>
#include <optional>
#include <variant>
#include <vector>

#include "Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Output : public Operator<T, V> {
  Output() = default;
  Output(string const& id, size_t numPorts = 1) : Operator<T, V>(id) {
    for (int i = 1; i <= numPorts; i++) {
      string inputPort = "i" + to_string(i);
      string outputPort = "o" + to_string(i);
      portsMap.emplace(inputPort, outputPort);
      this->addDataInput(inputPort, 1);
      this->addOutput(outputPort);
    }
  }

  size_t getNumPorts() const { return this->dataInputs.size(); }

  string typeName() const override { return "Output"; }

  virtual PortPayload<T, V> processData() override {
    PortPayload<T, V> outputMsgs;
    while (!this->toProcess.empty()) {
      string inputPort = *(this->toProcess.begin());
      Message<T, V> out = this->getDataInputLastMessage(inputPort);
      Messages<T, V> v;
      v.push_back(out);
      outputMsgs.emplace(portsMap.find(inputPort)->second, v);
      this->toProcess.erase(inputPort);
    }
    return outputMsgs;
  }

 private:
  map<string, string> portsMap;
};

}  // end namespace rtbot

#endif  // OUTPUT_H
