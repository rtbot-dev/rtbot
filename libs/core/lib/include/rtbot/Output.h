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

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    vector<Message<T, V>> v;
    v.push_back(out);
    outputMsgs.emplace(portsMap.find(inputPort)->second, v);
    return outputMsgs;
  }

 private:
  map<string, string> portsMap;
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

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> msg = this->getDataInputLastMessage(inputPort);
    *out = msg;
    vector<Message<T, V>> v;
    v.push_back(msg);
    outputMsgs.emplace(portsMap.find(inputPort)->second, v);
    return outputMsgs;
  }

 private:
  map<string, string> portsMap;
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

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> msg = this->getDataInputLastMessage(inputPort);
    (*out) << this->id << " " << msg << "\n";
    vector<Message<T, V>> v;
    v.push_back(msg);
    outputMsgs.emplace(portsMap.find(inputPort)->second, v);
    return outputMsgs;
  }

 private:
  map<string, string> portsMap;
};

}  // end namespace rtbot

#endif  // OUTPUT_H
