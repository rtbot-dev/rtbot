#ifndef INPUT_H
#define INPUT_H

#include "Message.h"
#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Input : public Operator<T, V> {
  Input() = default;

  Input(string const &id, size_t numPorts = 1) : Operator<T, V>(id) {
    for (int i = 1; i <= numPorts; i++) {
      string inputPort = "i" + to_string(i);
      string outputPort = "o" + to_string(i);
      portsMap.emplace(inputPort, outputPort);
      this->addDataInput(inputPort, 1);
      this->addOutput(outputPort);
    }
  }

  virtual Bytes collect() {
    Bytes bytes = Operator<T, V>::collect();
    // Serialize the lastSent map
    size_t lastSentSize = this->lastSent.size();
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char *>(&lastSentSize),
                 reinterpret_cast<const unsigned char *>(&lastSentSize) + sizeof(lastSentSize));

    for (const auto &[port, msg] : this->lastSent) {
      // Serialize the size of the port word
      size_t portSize = port.size();
      bytes.insert(bytes.end(), reinterpret_cast<const unsigned char *>(&portSize),
                   reinterpret_cast<const unsigned char *>(&portSize) + sizeof(portSize));
      bytes.insert(bytes.end(), port.begin(), port.end());

      // Serialize the message
      Bytes msgBytes = msg.collect();
      bytes.insert(bytes.end(), msgBytes.begin(), msgBytes.end());
    }

    return bytes;
  }

  virtual void restore(Bytes::const_iterator &it) {
    Operator<T, V>::restore(it);

    // Deserialize the lastSent map
    size_t lastSentSize = *reinterpret_cast<const size_t *>(&(*it));
    it += sizeof(lastSentSize);

    for (size_t i = 0; i < lastSentSize; i++) {
      // Deserialize the size of the port word
      size_t portSize = *reinterpret_cast<const size_t *>(&(*it));
      it += sizeof(portSize);
      string port(it, it + portSize);
      it += portSize;

      Message<T, V> msg;
      msg.restore(it);
      this->lastSent.erase(port);
      this->lastSent.insert({port, msg});
    }
  }

  virtual string debug(string empty) const override {
    string toReturn = "lastSent: ";
    toReturn += "[";
    for (const auto &[port, msg] : this->lastSent) {
      toReturn += port + ": " + msg.debug() + " ";
    }
    toReturn += "]";
    return Operator<T, V>::debug(toReturn);
  }

  size_t getNumPorts() const { return this->dataInputs.size(); }

  string typeName() const override { return "Input"; }

  virtual OperatorMessage<T, V> processData() override {
    OperatorMessage<T, V> outputMsgs;
    while (!this->toProcess.empty()) {
      string inputPort = *(this->toProcess.begin());
      Message<T, V> m0 = this->getDataInputMessage(inputPort, 0);
      if (this->lastSent.count(inputPort) > 0) {
        Message last = this->lastSent.at(inputPort);
        if (last.time < m0.time) {
          PortMessage<T, V> v;
          v.push_back(m0);
          outputMsgs.emplace(portsMap.find(inputPort)->second, v);
          this->lastSent.erase(inputPort);
          this->lastSent.emplace(inputPort, m0);
        }
      } else {
        PortMessage<T, V> v;
        v.push_back(m0);
        outputMsgs.emplace(portsMap.find(inputPort)->second, v);
        this->lastSent.emplace(inputPort, m0);
      }
      this->toProcess.erase(inputPort);
    }
    return outputMsgs;
  }

 private:
  map<string, string> portsMap;
  map<string, Message<T, V>> lastSent;
};

}  // namespace rtbot

#endif  // INPUT_H
