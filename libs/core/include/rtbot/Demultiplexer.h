#ifndef DEMULTIPLEXER_H
#define DEMULTIPLEXER_H

#include "Operator.h"

namespace rtbot {

template <class T, class V>
class Demultiplexer : public Operator<T, V> {
 public:
  Demultiplexer() = default;
  Demultiplexer(string const &id, size_t numPorts = 1) : Operator<T, V>(id) {
    if (numPorts < 1)
      throw std::runtime_error(typeName() + ": number of output ports have to be greater than or equal 1");

    for (int i = 1; i <= numPorts; i++) {
      string controlPort = string("c") + to_string(i);
      string outputPort = string("o") + to_string(i);
      this->addControlInput(controlPort);
      this->addOutput(outputPort);
      this->controlMap.emplace(controlPort, outputPort);
    }
    this->addDataInput("i1");
  }
  virtual ~Demultiplexer() = default;

  virtual string typeName() const override { return "Demultiplexer"; }

  void receiveData(Message<T, V> msg, string inputPort = "") override {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (this->dataInputs.count(inputPort) > 0) {
      this->dataInputs.find(inputPort)->second.push_back(msg);
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() +
                                                      this->dataInputs.find(inputPort)->second.back().value);
    } else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input data port");
  }

  virtual OperatorPayload<T, V> executeData() override {
    auto toEmit = processData();
    if (!toEmit.empty()) return this->emit(toEmit);
    return {};
  }

  virtual void receiveControl(Message<T, V> msg, string inputPort) {
    if (inputPort.empty()) {
      auto in = this->getControlInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (this->controlInputs.count(inputPort) > 0) {
      this->controlInputs.find(inputPort)->second.push_back(msg);
      this->controlInputs.find(inputPort)->second.setSum(this->controlInputs.find(inputPort)->second.getSum() +
                                                         this->controlInputs.find(inputPort)->second.back().value);

    } else
      throw std::runtime_error(typeName() + ": " + inputPort + " : refers to a non existing input control port");
  }

  virtual OperatorPayload<T, V> executeControl() override {
    auto toEmit = processControl();
    if (!toEmit.empty()) return this->emit(toEmit);
    return {};
  }

  virtual PortPayload<T, V> processData() { return join(); }

  virtual PortPayload<T, V> processControl() { return join(); }

 private:
  map<string, string> controlMap;

  PortPayload<T, V> join() {
    PortPayload<T, V> outputMsgs;

    vector<string> in = this->getDataInputs();
    string inputPort;
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw std::runtime_error(typeName() + ": " + "should have 1 input port, no more no less");

    if (this->dataInputs.find(inputPort)->second.empty()) return {};

    size_t instructions = 0;

    for (auto it = this->controlInputs.begin(); it != this->controlInputs.end(); ++it) {
      while (!it->second.empty() && (it->second.front().time < this->dataInputs.find(inputPort)->second.front().time)) {
        it->second.setSum(it->second.getSum() - it->second.front().value);
        it->second.pop_front();
      }
      if (!it->second.empty() && it->second.front().time == this->dataInputs.find(inputPort)->second.front().time &&
          (it->second.front().value == 1 || it->second.front().value == 0))
        instructions++;
      else if (!it->second.empty())
        throw std::runtime_error(
            typeName() + ": " +
            "Count not find matching control message for current message on the pipe. Review your design");
    }

    if (instructions == this->getNumControlInputs()) {
      for (auto it = this->controlInputs.begin(); it != this->controlInputs.end(); ++it) {
        if (it->second.front().value == 1) {
          Messages<T, V> v;
          v.push_back(this->dataInputs.find(inputPort)->second.front());
          outputMsgs.emplace(this->controlMap.find(it->first)->second, v);
        }
      }
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() -
                                                      this->dataInputs.find(inputPort)->second.front().value);
      this->dataInputs.find(inputPort)->second.pop_front();
      for (auto it = this->controlInputs.begin(); it != this->controlInputs.end(); ++it) {
        it->second.setSum(it->second.getSum() - it->second.front().value);
        it->second.pop_front();
      }
    }

    if (outputMsgs.size() > 0) return outputMsgs;

    return {};
  }
};

}  // end namespace rtbot

#endif  // DEMULTIPLEXER_H
