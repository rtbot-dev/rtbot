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

/**
 * @jsonschema
 * type: object
 * description: |
 *   `Output` operators are used to pull data out of the program.
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   numPorts:
 *     type: integer
 *     description: The number of ports.
 *     default: 1
 *     minimum: 1
 * required: ["id"]
 */

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

  virtual map<string, vector<Message<T, V>>> processData() override {
    map<string, vector<Message<T, V>>> outputMsgs;
    while (!this->toProcess.empty()) {
      string inputPort = *(this->toProcess.begin());
      Message<T, V> out = this->getDataInputLastMessage(inputPort);
      vector<Message<T, V>> v;
      v.push_back(out);
      outputMsgs.emplace(portsMap.find(inputPort)->second, v);
      toProcess.erase(inputPort);
    }
    return outputMsgs;
  }

 private:
  map<string, string> portsMap;
  set<string> toProcess;
};

}  // end namespace rtbot

#endif  // OUTPUT_H
