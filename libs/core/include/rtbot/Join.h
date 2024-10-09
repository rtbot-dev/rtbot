#ifndef JOIN_H
#define JOIN_H

#include "Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
class Join : public Operator<T, V> {
 public:
  Join() = default;
  Join(string const &id) : Operator<T, V>(id) {}
  Join(string const &id, size_t numPorts) : Operator<T, V>(id) {
    if (numPorts < 2) throw runtime_error(typeName() + ": number of ports have to be greater than or equal 2");

    for (int i = 1; i <= numPorts; i++) {
      string inputPort = string("i") + to_string(i);
      string outputPort = string("o") + to_string(i);

      this->addDataInput(inputPort, 0);
      this->addOutput(outputPort);
      this->controlMap.emplace(inputPort, outputPort);
    }
  }
  virtual ~Join() = default;

  virtual string typeName() const override { return "Join"; }

  void receiveData(Message<T, V> msg, string inputPort = "") override {
    if (inputPort.empty()) {
      throw runtime_error(typeName() + " : inputPort have to be specified");
    }

    if (this->dataInputs.count(inputPort) > 0) {
      this->dataInputs.find(inputPort)->second.push_back(msg);
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() +
                                                      this->dataInputs.find(inputPort)->second.back().value);
    } else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
  }

  virtual OperatorPayload<T, V> executeData() override {
    string inputPort;
    auto in = this->getDataInputs();
    inputPort = in.at(0);

    if (this->dataInputs.find(inputPort)->second.empty()) return {};
    T latest = this->dataInputs.find(inputPort)->second.front().time;

    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      if (it->second.empty())
        return {};
      else if (it->second.front().time > latest) {
        inputPort = it->first;
        latest = it->second.front().time;
      }
    }

    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      if (it->first == inputPort) continue;
      while (!it->second.empty() && (it->second.front().time < this->dataInputs.find(inputPort)->second.front().time)) {
        it->second.setSum(it->second.getSum() - it->second.front().value);
        it->second.pop_front();
      }
    }

    bool all_ready = true;
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      if (it->second.empty() || (it->second.front().time > this->dataInputs.find(inputPort)->second.front().time)) {
        all_ready = false;
        break;
      }
    }

    if (all_ready) {
      auto toEmit = processData();
      for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
        it->second.setSum(it->second.getSum() - it->second.front().value);
        it->second.pop_front();
      }
      return this->emit(toEmit);
    }
    return {};
  }

  virtual PortPayload<T, V> processData() {
    PortPayload<T, V> outputMsgs;
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      Message<T, V> out = it->second.front();
      Messages<T, V> v;
      v.push_back(out);
      outputMsgs.emplace(this->controlMap.find(it->first)->second, v);
    }

    return outputMsgs;
  }

 protected:
  map<string, string> controlMap;
};

}  // end namespace rtbot

#endif  // JOIN_H
