#ifndef OPERATOR_H
#define OPERATOR_H

#include <algorithm>
#include <deque>
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
  struct InputPolicy {
   private:
    bool eager;

   public:
    InputPolicy(bool eager = false) { this->eager = eager; }
    bool isEager() const { return this->eager; }
  };

  struct Input {
    Input(string id, size_t max = 0, InputPolicy policy = {}) {
      this->id = id;
      this->max = (max <= 0) ? numeric_limits<size_t>::max() : max;
      this->policy = policy;
      this->sum = 0;
    }
    bool isEager() const { return policy.isEager(); }
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
    InputPolicy getPolicy() const { return policy; }

   private:
    string id;
    deque<Message<T, V>> data;
    size_t max;
    InputPolicy policy;
    V sum;
  };

  string id;
  map<string, Input> dataInputs;
  map<string, Input> controlInputs;
  set<string> outputIds;
  map<string, vector<Connection>> outputs;

  Operator() = default;
  explicit Operator(string const& id) { this->id = id; }
  virtual ~Operator() = default;

  virtual string typeName() const = 0;

  virtual map<string, vector<Message<T, V>>> processData(string inputPort) = 0;

  virtual map<string, vector<Message<T, V>>> processControl(string inputPort) {
    // TODO: implement reset here
    return {};
  }

  map<string, typename Operator<T, V>::InputPolicy> getDataPolicies() const {
    map<string, InputPolicy> out;
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      out.emplace(it->first, it->second.getPolicy());
    }
    return out;
  }

  map<string, typename Operator<T, V>::InputPolicy> getControlPolicies() const {
    map<string, InputPolicy> out;
    for (auto it = this->controlInputs.begin(); it != this->controlInputs.end(); ++it) {
      out.emplace(it->first, it->second.getPolicy());
    }
    return out;
  }

  V getDataInputSum(string inputPort) {
    if (dataInputs.count(inputPort) > 0)
      return dataInputs.find(inputPort)->second.getSum();
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return 0;
  }

  bool isDataInputEager(string inputPort = "") {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }
    if (dataInputs.count(inputPort) > 0)
      return dataInputs.find(inputPort)->second.isEager();
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
  }

  size_t getDataInputMaxSize(string inputPort = "") const {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (dataInputs.count(inputPort) > 0)
      return dataInputs.find(inputPort)->second.getMaxSize();
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
  }

  size_t getDataInputSize(string inputPort = "") {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (dataInputs.count(inputPort) > 0)
      return dataInputs.find(inputPort)->second.size();
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return 0;
  }

  Message<T, V> getDataInputMessage(string inputPort, size_t index) {
    if (dataInputs.count(inputPort) > 0)
      if (!dataInputs.find(inputPort)->second.empty())
        return dataInputs.find(inputPort)->second.at(index);
      else
        return {};
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return {};
  }

  Message<T, V> getDataInputLastMessage(string inputPort) {
    if (dataInputs.count(inputPort) > 0)
      if (!dataInputs.find(inputPort)->second.empty())
        return dataInputs.find(inputPort)->second.back();
      else
        return {};
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return {};
  }

  Message<T, V> getDataInputFirstMessage(string inputPort) {
    if (dataInputs.count(inputPort) > 0)
      if (!dataInputs.find(inputPort)->second.empty())
        return dataInputs.find(inputPort)->second.front();
      else
        return {};
    else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
    return {};
  }

  bool allDataInputPortsFull() const {
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      if (it->second.getMaxSize() > it->second.size()) return false;
    }
    return true;
  }

  bool allControlInputPortsFull() const {
    for (auto it = this->controlInputs.begin(); it != this->controlInputs.end(); ++it) {
      if (it->second.getMaxSize() > it->second.size()) return false;
    }
    return true;
  }

  virtual map<string, vector<Message<T, V>>> receiveData(Message<T, V> const& msg, string inputPort = "") {
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

      if (allDataInputPortsFull()) {
        return this->processData(inputPort);
      }
      return {};
    } else
      throw std::runtime_error(typeName() + ": " + inputPort + " : refers to a non existing input port");
    return {};
  }

  virtual map<string, vector<Message<T, V>>> receiveControl(Message<T, V> const& msg, string inputPort) {
    if (this->controlInputs.count(inputPort) > 0) {
      if (this->controlInputs.find(inputPort)->second.getMaxSize() ==
          this->controlInputs.find(inputPort)->second.size()) {
        this->controlInputs.find(inputPort)->second.setSum(this->controlInputs.find(inputPort)->second.getSum() -
                                                           this->controlInputs.find(inputPort)->second.front().value);
        this->controlInputs.find(inputPort)->second.pop_front();
      } else if (this->controlInputs.find(inputPort)->second.getMaxSize() <
                 this->controlInputs.find(inputPort)->second.size())
        throw std::runtime_error(typeName() + ": " + inputPort + " : went above maximum size");

      this->controlInputs.find(inputPort)->second.push_back(msg);
      this->controlInputs.find(inputPort)->second.setSum(this->controlInputs.find(inputPort)->second.getSum() +
                                                         this->controlInputs.find(inputPort)->second.back().value);

      auto toEmit = this->processControl(inputPort);

      return toEmit;

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
          if (connection.fOperator->isDataInput(connection.inputPort))
            mergeOutput(out, connection.fOperator->receiveData(msg, connection.inputPort));
          else if (connection.fOperator->isControlInput(connection.inputPort)) {
            mergeOutput(out, connection.fOperator->receiveControl(msg, connection.inputPort));
          }
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

  Operator<T, V>* addDataInput(string inputId, size_t max = 0, InputPolicy policy = {}) {
    if (inputId.empty()) {
      throw std::runtime_error(typeName() + " : input port have to be specified");
    }
    if (dataInputs.count(inputId) == 0 && controlInputs.count(inputId) == 0) {
      dataInputs.emplace(inputId, Input(inputId, max, policy));
      return this;
    } else
      throw std::runtime_error(typeName() + ": " + inputId + " refers to an already existing input port");
  }

  Operator<T, V>* addControlInput(string inputId, size_t max = 0, InputPolicy policy = {}) {
    if (inputId.empty()) {
      throw std::runtime_error(typeName() + " : control port have to be specified");
    }
    if (dataInputs.count(inputId) == 0 && controlInputs.count(inputId) == 0) {
      controlInputs.emplace(inputId, Input(inputId, max, policy));
      return this;
    } else
      throw std::runtime_error(typeName() + ": " + inputId + " refers to an already existing input port");
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

  Operator<T, V>* connect(Operator<T, V>& child, string outputPort = "", string inputPort = "") {
    return connect(&child, outputPort, inputPort);
  }

  Operator<T, V>* connect(Operator<T, V>* child, string outputPort = "", string inputPort = "") {
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

    if (inputPort.empty()) throw std::runtime_error(child->typeName() + ": input port found empty");
    if (outputPort.empty()) throw std::runtime_error(typeName() + ": output port found empty");

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
