#ifndef DEMULTIPLEXER_H
#define DEMULTIPLEXER_H

#include "Operator.h"

namespace rtbot {

/**
 * class Join is responsible for synchronizing many channels of Messages. This is a simple and intuitive implementation.
 * It uses as many queues as channels.
 *
 * The queues are updated every time a new message arrives:
 * 1. the old messages are removed.
 * 2. If all the incoming messages of the queues match the current time stamp, then a message is generated by
 * concatenating them.
 *
 * To implement any Operator that requires synchronizing many channels of messages
 * the user should just inherit from Join and override the method processData(msg) where the ready-to-use message msg is
 * given.
 */
template <class T, class V>
class Demultiplexer : public Operator<T, V> {
 public:
  Demultiplexer() = default;
  Demultiplexer(string const &id, size_t numOutputPorts = 2) : Operator<T, V>(id) {
    if (numOutputPorts < 2)
      throw std::runtime_error(typeName() + ": number of output ports have to be greater than or equal 2");

    for (int i = 1; i <= numOutputPorts; i++) {
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

  map<string, std::vector<Message<T, V>>> receiveData(Message<T, V> const &msg, string inputPort = "") override {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (this->dataInputs.count(inputPort) > 0) {
      this->dataInputs.find(inputPort)->second.push_back(msg);
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() +
                                                      this->dataInputs.find(inputPort)->second.back().value);
    } else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");

    auto toEmit = processData(inputPort);

    return this->emit(toEmit);
  }

  virtual map<string, vector<Message<T, V>>> receiveControl(Message<T, V> const &msg, string inputPort) {
    if (this->controlInputs.count(inputPort) > 0) {
      this->controlInputs.find(inputPort)->second.push_back(msg);
      this->controlInputs.find(inputPort)->second.setSum(this->controlInputs.find(inputPort)->second.getSum() +
                                                         this->controlInputs.find(inputPort)->second.back().value);

    } else
      throw std::runtime_error(typeName() + ": " + inputPort + " : refers to a non existing input port");

    auto toEmit = this->processControl(inputPort);

    return this->emit(toEmit);
  }

  /*
    map<outputPort, vector<Message<T, V>>>
  */
  virtual map<string, vector<Message<T, V>>> processData(string inputPort) { return join(); }

  /*
      map<outputPort, vector<Message<T, V>>>
  */
  virtual map<string, vector<Message<T, V>>> processControl(string inputPort) { return join(); }

 private:
  map<string, string> controlMap;

  /*
      map<outputPort, vector<Message<T, V>>>
  */
  map<string, vector<Message<T, V>>> join() {
    map<string, vector<Message<T, V>>> outputMsgs;

    vector<string> in = this->getDataInputs();
    string inputPort;
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw std::runtime_error(typeName() + ": " + "should have 1 input port, no more no less");

    if (this->dataInputs.find(inputPort)->second.empty()) return {};

    for (auto it = this->controlInputs.begin(); it != this->controlInputs.end(); ++it) {
      while (!it->second.empty() && (it->second.front().time < this->dataInputs.find(inputPort)->second.front().time)) {
        it->second.setSum(it->second.getSum() - it->second.front().value);
        it->second.pop_front();
      }
      if (!it->second.empty() && it->second.front().time == this->dataInputs.find(inputPort)->second.front().time) {
        vector<Message<T, V>> v;
        v.push_back(this->dataInputs.find(inputPort)->second.front());
        outputMsgs.emplace(this->controlMap.find(it->first)->second, v);
        it->second.setSum(it->second.getSum() - it->second.front().value);
        it->second.pop_front();
        this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() -
                                                        this->dataInputs.find(inputPort)->second.front().value);
        this->dataInputs.find(inputPort)->second.pop_front();
        return outputMsgs;
      }
    }

    return {};
  }
};

}  // end namespace rtbot

#endif  // DEMULTIPLEXER_H