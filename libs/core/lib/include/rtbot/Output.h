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

using std::function;

template <class T, class V>
std::ostream& operator<<(std::ostream& out, Message<T, V> const& msg) {
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

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> toEmit = this->getDataInputLastMessage(inputPort);
    return this->emit(toEmit, {portsMap.find(inputPort)->second});
  }

 private:
  map<string, string> portsMap;
};

template <class T, class V>
struct Output_opt : public Operator<T, V> {
  std::optional<Message<T, V>>* out = nullptr;

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

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> toEmit = this->getDataInputLastMessage(inputPort);
    *out = toEmit;
    return this->emit(toEmit, {portsMap.find(inputPort)->second});
  }

 private:
  map<string, string> portsMap;
};

template <class T, class V>
struct Output_os : public Operator<T, V> {
  std::ostream* out = nullptr;

  Output_os() = default;
  Output_os(string const& id, std::ostream& out, size_t numPorts = 1) : Operator<T, V>(id) {
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

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> toEmit = this->getDataInputLastMessage(inputPort);
    (*out) << this->id << " " << toEmit << "\n";
    return this->emit(toEmit, {portsMap.find(inputPort)->second});
  }

 private:
  map<string, string> portsMap;
};

}  // end namespace rtbot

#endif  // OUTPUT_H
