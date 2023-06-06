#ifndef JOIN_H
#define JOIN_H

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
class Join : public Operator<T, V> {
 private:
  map<string, string> fromTo;

 public:
  Join() = default;
  Join(string const &id_, size_t numPorts_, map<string, typename Operator<T, V>::InputPolicy> _policies = {})
      : Operator<T, V>(id_) {
    if (numPorts_ < 2) throw std::runtime_error(typeName() + ": number of ports have to be greater than or equal 2");

    for (int i = 1; i <= numPorts_; i++) {
      string inputPort = string("i") + to_string(i);
      string outputPort = string("o") + to_string(i);
      if (_policies.count(inputPort) > 0)
        this->addInput(inputPort, 0, _policies.find(inputPort)->second);
      else
        this->addInput(inputPort);
      this->addOutput(outputPort);
      fromTo.emplace(inputPort, outputPort);
    }
  }
  virtual ~Join() = default;

  virtual string typeName() const override { return "Join"; }

  map<string, std::vector<Message<T, V>>> receive(Message<T, V> const &msg, string inputPort = "") override {
    if (inputPort.empty()) {
      throw std::runtime_error(typeName() + " : inputPort have to be specified");
    }

    if (this->inputs.count(inputPort) > 0) {
      if (this->inputs.find(inputPort)->second.isEager() && !this->inputs.find(inputPort)->second.empty()) {
        this->inputs.find(inputPort)->second.pop_front();
      }
      this->inputs.find(inputPort)->second.push_back(msg);
    } else
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");

    for (auto it = this->inputs.begin(); it != this->inputs.end(); ++it) {
      if (it->first == inputPort || it->second.isEager()) continue;
      while (!it->second.empty() && it->second.front().time < msg.time) it->second.pop_front();
    }

    bool all_ready = true;
    for (auto it = this->inputs.begin(); it != this->inputs.end(); ++it) {
      if (it->second.empty() || (it->second.front().time > msg.time && !it->second.isEager())) all_ready = false;
    }

    if (all_ready) {
      auto toEmit = processData(inputPort);
      for (auto it = this->inputs.begin(); it != this->inputs.end(); ++it) {
        if (!it->second.isEager()) it->second.pop_front();
      }
      return this->emit(toEmit);
    }
    return {};
  }

  /*
    map<outputPort, vector<Message<T, V>>>
  */
  virtual map<string, vector<Message<T, V>>> processData(string inputPort) {
    map<string, vector<Message<T, V>>> outputMsgs;

    for (auto it = this->inputs.begin(); it != this->inputs.end(); ++it) {
      vector<Message<T, V>> v;
      v.push_back(this->inputs.find(it->first)->second.front());
      outputMsgs.emplace(fromTo.find(it->first)->second, v);
    }

    return outputMsgs;
  }
};

}  // end namespace rtbot

#endif  // JOIN_H
