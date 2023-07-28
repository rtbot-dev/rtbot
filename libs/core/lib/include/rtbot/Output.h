#ifndef OUTPUT_H
#define OUTPUT_H

#include <fstream>
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
ostream& operator<<(ostream& out, Message<T, V> const& msg) {
  out << msg.time << " " << msg.value;
  return out;
}

template <class T, class V>
struct Output_vec : public Operator<T, V> {
  Output_vec() = default;
  Output_vec(string const& id, size_t numPorts = 1) : Operator<T, V>(id) {
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

template <class T, class V>
struct Output_opt : public Operator<T, V> {
  optional<Message<T, V>>* out = nullptr;

  Output_opt() = default;
  Output_opt(string const& id, size_t numPorts = 1) : Operator<T, V>(id) {
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
      Message<T, V> msg = this->getDataInputLastMessage(inputPort);
      *out = msg;
      vector<Message<T, V>> v;
      v.push_back(msg);
      outputMsgs.emplace(portsMap.find(inputPort)->second, v);
      toProcess.erase(inputPort);
    }
    return outputMsgs;
  }

 private:
  map<string, string> portsMap;
  set<string> toProcess;
};

template <class T, class V>
struct Output_os : public Operator<T, V> {
  ostream* out = nullptr;

  Output_os() = default;
  Output_os(string const& id, ostream& out, size_t numPorts = 1) : Operator<T, V>(id) {
    this->out = &out;
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
      Message<T, V> msg = this->getDataInputLastMessage(inputPort);
      (*out) << this->id << " " << msg << "\n";
      vector<Message<T, V>> v;
      v.push_back(msg);
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
