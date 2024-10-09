#ifndef OPERATOR_H
#define OPERATOR_H

#include <algorithm>
#include <deque>
#include <iostream>
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
 * @tparam T Numeric type used for integer computations, (`int`, `int64`, etc.).
 * @tparam V Numeric type used for floating computations, (`float`, `double`, etc.).
 */

template <class T, class V>
class Operator;

// useful type aliases
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
  struct Input {
    Input(string id, size_t max = 0) {
      this->id = id;
      this->max = (max <= 0) ? numeric_limits<size_t>::max() : max;
      this->sum = 0;
    }

    Message<T, V> front() { return data.front(); }
    Message<T, V> back() { return data.back(); }
    Message<T, V> at(size_t index) { return data.at(index); }
    bool empty() { return data.empty(); }
    size_t size() const { return data.size(); }
    void push_back(Message<T, V> msg) { data.push_back(msg); }
    void pop_front() { data.pop_front(); }
    void pop_back() { data.pop_back(); }
    V getSum() { return sum; }
    void setSum(V value) { sum = value; }
    size_t getMaxSize() const { return max; }

   private:
    string id;
    deque<Message<T, V>> data;
    size_t max;
    V sum;
  };

  string id;
  map<string, Input> dataInputs;
  map<string, Input> controlInputs;
  set<string> outputIds;
  map<string, vector<Connection>> outputs;
  set<string> toProcess;

  Operator() = default;
  explicit Operator(string const& id) { this->id = id; }
  virtual ~Operator() = default;

  void friend swap(Operator& op1, Operator& op2) {
    std::swap(op1.id, op2.id);
    std::swap(op1.dataInputs, op2.dataInputs);
    std::swap(op1.controlInputs, op2.controlInputs);
    std::swap(op1.outputIds, op2.outputIds);
    std::swap(op1.outputs, op2.outputs);
    std::swap(op1.toProcess, op2.toProcess);
  }

  virtual string typeName() const = 0;

  virtual PortPayload<T, V> processData() = 0;

  virtual PortPayload<T, V> processControl() {
    // TODO: implement reset here
    return {};
  }

  V getDataInputSum(string inputPort) {
    if (dataInputs.count(inputPort) > 0)
      return dataInputs.find(inputPort)->second.getSum();
    else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return 0;
  }

  size_t getDataInputMaxSize(string inputPort = "") const {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (dataInputs.count(inputPort) > 0)
      return dataInputs.find(inputPort)->second.getMaxSize();
    else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
  }

  size_t getDataInputSize(string inputPort = "") {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (this->dataInputs.count(inputPort) > 0)
      return this->dataInputs.find(inputPort)->second.size();
    else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return 0;
  }

  Message<T, V> getDataInputMessage(string inputPort, size_t index) {
    if (this->dataInputs.count(inputPort) > 0) {
      auto result = this->dataInputs.find(inputPort);
      if (!result->second.empty())
        return result->second.at(index);
      else
        return {};
    } else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return {};
  }

  Message<T, V> getDataInputLastMessage(string inputPort) {
    if (this->dataInputs.count(inputPort) > 0) {
      auto result = this->dataInputs.find(inputPort);
      if (!result->second.empty())
        return result->second.back();
      else
        return {};
    } else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return {};
  }

  Message<T, V> getControlInputLastMessage(string controlPort) {
    if (this->controlInputs.count(controlPort) > 0) {
      auto result = this->controlInputs.find(controlPort);
      if (!result->second.empty())
        return result->second.back();
      else
        return {};
    } else
      throw runtime_error(typeName() + ": " + controlPort + " refers to a non existing control port");
    return {};
  }

  Message<T, V> getControlInputMessage(string controlPort, size_t index) {
    if (this->controlInputs.count(controlPort) > 0) {
      auto result = this->controlInputs.find(controlPort);
      if (!result->second.empty())
        return result->second.at(index);
      else
        return {};
    } else
      throw runtime_error(typeName() + ": " + controlPort + " refers to a non existing control port");
    return {};
  }

  Message<T, V> getDataInputFirstMessage(string inputPort) {
    if (this->dataInputs.count(inputPort) > 0)
      if (!this->dataInputs.find(inputPort)->second.empty())
        return this->dataInputs.find(inputPort)->second.front();
      else
        return {};
    else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return {};
  }

  virtual void receiveData(Message<T, V> msg, string inputPort = "") {
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
        throw runtime_error(typeName() + ": " + inputPort + " : went above maximum size");

      this->dataInputs.find(inputPort)->second.push_back(msg);
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() +
                                                      this->dataInputs.find(inputPort)->second.back().value);
      if (this->toProcess.count(inputPort) == 0) this->toProcess.insert(inputPort);
    } else
      throw runtime_error(typeName() + ": " + inputPort + " : refers to a non existing input port");
  }

  virtual OperatorPayload<T, V> executeData() {
    vector<string> toRemove;
    for (auto it = this->toProcess.begin(); it != this->toProcess.end(); ++it) {
      if (this->getDataInputMaxSize(*it) > this->getDataInputSize(*it)) toRemove.push_back(*it);
    }
    for (int i = 0; i < toRemove.size(); i++) this->toProcess.erase(toRemove.at(i));

    if (!this->toProcess.empty()) {
      auto toEmit = processData();
      this->toProcess.clear();
      if (!toEmit.empty()) return this->emit(toEmit);
    }
    return {};
  }

  virtual void receiveControl(Message<T, V> msg, string inputPort) {
    if (inputPort.empty()) {
      auto in = this->getControlInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (this->controlInputs.count(inputPort) > 0) {
      if (this->controlInputs.find(inputPort)->second.getMaxSize() ==
          this->controlInputs.find(inputPort)->second.size()) {
        this->controlInputs.find(inputPort)->second.setSum(this->controlInputs.find(inputPort)->second.getSum() -
                                                           this->controlInputs.find(inputPort)->second.front().value);
        this->controlInputs.find(inputPort)->second.pop_front();
      } else if (this->controlInputs.find(inputPort)->second.getMaxSize() <
                 this->controlInputs.find(inputPort)->second.size())
        throw runtime_error(typeName() + ": " + inputPort + " : went above maximum size");

      this->controlInputs.find(inputPort)->second.push_back(msg);
      this->controlInputs.find(inputPort)->second.setSum(this->controlInputs.find(inputPort)->second.getSum() +
                                                         this->controlInputs.find(inputPort)->second.back().value);

    } else
      throw runtime_error(typeName() + ": " + inputPort + " : refers to a non existing input port");
  }

  virtual OperatorPayload<T, V> executeControl() {
    auto toEmit = this->processControl();
    if (!toEmit.empty())
      return this->emit(toEmit);
    else
      return {};
  }

  bool hasControlInputs() { return !this->getControlInputs().empty(); }

  OperatorPayload<T, V> emit(Message<T, V> msg, vector<string> outputPorts = {}) const {
    OperatorPayload<T, V> out;

    if (outputPorts.empty()) outputPorts = this->getOutputs();
    for (auto outputPort : outputPorts) {
      Messages<T, V> v;
      v.push_back(msg);
      if (out.count(this->id) == 0) {
        PortPayload<T, V> msgs;
        msgs.emplace(outputPort, v);
        out.emplace(this->id, msgs);
      } else
        out.find(this->id)->second.emplace(outputPort, v);
      if (this->outputs.count(outputPort) > 0) {
        for (auto connection : outputs.find(outputPort)->second) {
          if (connection.fOperator->isDataInput(connection.inputPort))
            connection.fOperator->receiveData(msg, connection.inputPort);
          else if (connection.fOperator->isControlInput(connection.inputPort)) {
            connection.fOperator->receiveControl(msg, connection.inputPort);
          }
        }
        for (auto connection : outputs.find(outputPort)->second) {
          if (connection.fOperator->isDataInput(connection.inputPort))
            mergeOutput(out, connection.fOperator->executeData());
          else if (connection.fOperator->isControlInput(connection.inputPort)) {
            mergeOutput(out, connection.fOperator->executeControl());
          }
        }
      }
    }
    return out;
  }

  OperatorPayload<T, V> emit(Messages<T, V> msgs, vector<string> outputPorts = {}) const {
    OperatorPayload<T, V> out;
    for (auto msg : msgs) {
      mergeOutput(out, emit(msg, outputPorts));
    }
    return out;
  }

  OperatorPayload<T, V> emit(PortPayload<T, V> outputMsgs) const {
    OperatorPayload<T, V> out;
    for (auto it = outputMsgs.begin(); it != outputMsgs.end(); ++it) {
      if (this->outputIds.count(it->first) > 0) {
        mergeOutput(out, emit(it->second, {it->first}));
      } else
        throw runtime_error(typeName() + ": " + it->first + " refers to a non existing output port");
    }
    return out;
  }

  Operator<T, V>* addDataInput(string inputId, size_t max = 0) {
    if (inputId.empty()) {
      throw runtime_error(typeName() + " : input port have to be specified");
    }
    if (dataInputs.count(inputId) == 0 && controlInputs.count(inputId) == 0) {
      dataInputs.emplace(inputId, Input(inputId, max));
      return this;
    } else
      throw runtime_error(typeName() + ": " + inputId + " refers to an already existing input port");
  }

  Operator<T, V>* addControlInput(string inputId, size_t max = 0) {
    if (inputId.empty()) {
      throw runtime_error(typeName() + " : control port have to be specified");
    }
    if (dataInputs.count(inputId) == 0 && controlInputs.count(inputId) == 0) {
      controlInputs.emplace(inputId, Input(inputId, max));
      return this;
    } else
      throw runtime_error(typeName() + ": " + inputId + " refers to an already existing input port");
  }

  vector<string> getDataInputs() const {
    vector<string> keys;
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      keys.push_back(it->first);
    }
    return keys;
  }

  vector<string> getControlInputs() const {
    vector<string> keys;
    for (auto it = this->controlInputs.begin(); it != this->controlInputs.end(); ++it) {
      keys.push_back(it->first);
    }
    return keys;
  }

  vector<string> getAllInputs() const {
    vector<string> keys = this->getDataInputs();
    for (auto it = this->controlInputs.begin(); it != this->controlInputs.end(); ++it) {
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

  bool isDataInput(string inputPort) const {
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      if (it->first == inputPort) return true;
    }
    return false;
  }

  bool isControlInput(string inputPort) const {
    for (auto it = this->controlInputs.begin(); it != this->controlInputs.end(); ++it) {
      if (it->first == inputPort) return true;
    }
    return false;
  }

  size_t getNumDataInputs() const { return this->dataInputs.size(); }

  size_t getNumOutputPorts() const { return this->outputIds.size(); }

  size_t getNumControlInputs() const { return this->controlInputs.size(); }

  virtual Operator<T, V>* connect(Operator<T, V>& child, string outputPort = "", string inputPort = "") {
    return connect(&child, outputPort, inputPort);
  }

  virtual Operator<T, V>* connect(Operator<T, V>* child, string outputPort = "", string inputPort = "") {
    vector<string> out = this->getOutputs();
    vector<string> in = child->getAllInputs();

    if (outputPort.empty()) {
      // auto assign if there is just one output
      if (out.size() == 1) outputPort = out.at(0);
    }

    if (inputPort.empty()) {
      // auto assign if there is just one input and is a data input
      if (in.size() == 1 && child->isDataInput(in.at(0))) inputPort = in.at(0);
    }

    if (inputPort.empty())
      throw runtime_error(child->typeName() + ": input port found empty and could not be deducted");
    if (outputPort.empty()) throw runtime_error(typeName() + ": output port found empty and could not be deducted");

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

  static void mergeOutput(OperatorPayload<T, V>& out, OperatorPayload<T, V> const& x) {
    for (const auto& [id, map] : x) {
      auto& mapOut = out[id];
      mergeOutput(mapOut, map);
    }
  }

  static void mergeOutput(PortPayload<T, V>& out, PortPayload<T, V> const& x) {
    for (const auto& [id, msgs] : x) {
      auto& vec = out[id];
      for (auto resultMessage : msgs) vec.push_back(resultMessage);
    }
  }

 private:
};

}  // end namespace rtbot

#endif  // OPERATOR_H
