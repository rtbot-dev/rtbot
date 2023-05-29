#ifndef OPERATOR_H
#define OPERATOR_H

#include <algorithm>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "rtbot/Message.h"

namespace rtbot {

using namespace std;

/**
 * Represents a generic operator that can receive a message and forward its
 * computed value to its children. This is one of the main building blocks of
 * rtbot framework.
 *
 * @tparam V Numeric type used for floating computations, (`float`, `double`,
 * etc.).
 */

template <class T, class V>
class Operator;
template <class T, class V>
using Op_ptr = unique_ptr<Operator<T, V>>;

template <class T, class V>
class Operator {
  /********************************/
  struct Connection {
    Operator<T, V>* fOperator;
    string inputPort;
  };
  /********************************/

 public:
  string id;
  map<string, deque<Message<T, V>>> inputs;
  set<string> outputIds;
  map<string, vector<Connection>> outputs;
  map<string, size_t> sizes;
  map<string, V> sum;

  Operator() = default;
  explicit Operator(string const& id_) : id(id_) {}
  virtual ~Operator() = default;

  virtual string typeName() const = 0;

  virtual map<string, vector<Message<T, V>>> processData(string inputPort) = 0;

  V getSum(string inputPort) {
    if (sum.count(inputPort) > 0)
      return sum.find(inputPort)->second;
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return 0;
  }

  size_t getMaxSize(string inputPort = "") const {
    if (inputPort.empty()) {
      auto in = this->getInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (sizes.count(inputPort) > 0)
      return sizes.find(inputPort)->second;
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
  }

  size_t getSize(string inputPort) {
    if (inputs.count(inputPort) > 0)
      return inputs.find(inputPort)->second.size();
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return 0;
  }

  Message<T, V> getMessage(string inputPort, size_t index) {
    if (inputs.count(inputPort) > 0)
      if (!inputs.find(inputPort)->second.empty())
        return inputs.find(inputPort)->second.at(index);
      else
        return {};
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return {};
  }

  Message<T, V> getLastMessage(string inputPort) {
    if (inputs.count(inputPort) > 0)
      if (!inputs.find(inputPort)->second.empty())
        return inputs.find(inputPort)->second.back();
      else
        return {};
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return {};
  }

  bool allInputPortsFull() const {
    for (auto it = this->inputs.begin(); it != this->inputs.end(); ++it) {
      if (sizes.find(it->first)->second > inputs.find(it->first)->second.size()) return false;
    }
    return true;
  }

  virtual map<string, vector<Message<T, V>>> receive(Message<T, V> const& msg, string inputPort = "") {
    if (inputPort.empty()) {
      auto in = this->getInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (inputs.count(inputPort) > 0 && sizes.count(inputPort) > 0 && sum.count(inputPort) > 0) {
      if (allInputPortsFull()) {
        sum.find(inputPort)->second = sum.find(inputPort)->second - inputs.find(inputPort)->second.front().value;
        inputs.find(inputPort)->second.pop_front();
      }

      inputs.find(inputPort)->second.push_back(msg);
      sum.find(inputPort)->second = sum.find(inputPort)->second + inputs.find(inputPort)->second.back().value;

      if (allInputPortsFull()) {
        return this->processData(inputPort);
      }
      return {};
    } else
      throw std::runtime_error(typeName() + ": " + inputPort + " : refers to a non existing input port");
    return {};
  }

  map<string, vector<Message<T, V>>> emit(Message<T, V> const& msg, vector<string> outputPorts = {}) const {
    map<string, vector<Message<T, V>>> out = {{id, {msg}}};
    if (outputPorts.empty()) outputPorts = this->getOutputs();
    for (auto outputPort : outputPorts) {
      if (outputs.count(outputPort) > 0) {
        for (auto connection : outputs.find(outputPort)->second) {
          mergeOutput(out, connection.fOperator->receive(msg, connection.inputPort));
        }
      }
    }
    return out;
  }

  map<string, vector<Message<T, V>>> emit(vector<Message<T, V>> const& msgs, vector<string> outputPorts = {}) const {
    map<string, vector<Message<T, V>>> out;
    for (const auto& msg : msgs) mergeOutput(out, emit(msg, outputPorts));
    return out;
  }

  map<string, vector<Message<T, V>>> emit(map<string, vector<Message<T, V>>> const& outputMsgs) const {
    map<string, vector<Message<T, V>>> out;
    for (auto it = outputMsgs.begin(); it != outputMsgs.end(); ++it) {
      if (this->outputIds.count(it->first) > 0) {
        mergeOutput(out, emit(it->second, {it->first}));
      } else
        throw std::runtime_error(typeName() + ": " + it->first + " refers to a non existing output port");
    }
    return out;
  }

  Operator<T, V>* addInput(string inputId, size_t max = 0) {
    inputs.emplace(inputId, deque<Message<T, V>>());
    if (max > 0) sizes.emplace(inputId, max);
    sum.emplace(inputId, 0);
    return this;
  }

  vector<string> getInputs() const {
    vector<string> keys;
    for (auto it = inputs.begin(); it != inputs.end(); ++it) {
      keys.push_back(it->first);
    }
    return keys;
  }

  Operator<T, V>* addOutput(string outputId) {
    outputIds.insert(outputId);
    return this;
  }

  vector<string> getOutputs() const {
    vector<string> vOutputs;
    vOutputs.assign(outputIds.begin(), outputIds.end());
    return vOutputs;
  }

  size_t getNumInputs() const { return this->inputs.size(); }

  Operator<T, V>* connect(Operator<T, V>& child, string outputPort = "", string inputPort = "") {
    return connect(&child, outputPort, inputPort);
  }

  Operator<T, V>* connect(Operator<T, V>* child, string outputPort = "", string inputPort = "") {
    vector<string> out = this->getOutputs();
    vector<string> in = child->getInputs();

    if (outputPort.empty()) {
      if (out.size() == 1) outputPort = out.at(0);
    }
    if (inputPort.empty()) {
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (find(out.begin(), out.end(), outputPort) != out.end() && find(in.begin(), in.end(), inputPort) != in.end()) {
      Connection c;
      c.fOperator = child;
      c.inputPort = inputPort;
      if (outputs.count(outputPort) == 0) {
        vector<Connection> v;
        v.push_back(c);
        outputs.emplace(outputPort, v);
      } else
        outputs.find(outputPort)->second.push_back(c);

      return child;
    } else
      return nullptr;
  }

 private:
  static void mergeOutput(map<string, vector<Message<T, V>>>& out, map<string, vector<Message<T, V>>> const& x) {
    for (const auto& [id, msgs] : x) {
      auto& vec = out[id];
      for (auto resultMessage : msgs) vec.push_back(resultMessage);
    }
  }
};

}  // end namespace rtbot

#endif  // OPERATOR_H
