#ifndef VARIABLE_H
#define VARIABLE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
class Variable : public Operator<T, V> {
 public:
  Variable() = default;

  Variable(string const &id, V value = 0) : Operator<T, V>(id) {
    this->value = value;
    this->initialized = false;
    this->addDataInput("i1");
    this->addControlInput("c1", 1);
    this->addOutput("o1");
  }

  virtual ~Variable() = default;

  virtual Bytes collect() {
    Bytes bytes = Operator<T, V>::collect();

    // Serialize value
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char *>(&value),
                 reinterpret_cast<const unsigned char *>(&value) + sizeof(value));

    // Serialize initialized
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char *>(&initialized),
                 reinterpret_cast<const unsigned char *>(&initialized) + sizeof(initialized));

    return bytes;
  }

  virtual void restore(Bytes::const_iterator &it) {
    Operator<T, V>::restore(it);

    // Deserialize value
    value = *reinterpret_cast<const V *>(&(*it));
    it += sizeof(value);

    // Deserialize initialized
    initialized = *reinterpret_cast<const bool *>(&(*it));
    it += sizeof(initialized);
  }

  virtual string typeName() const override { return "Variable"; }

  V getValue() const { return this->value; }

  ProgramMessage<T, V> executeData() override {
    auto toEmit = this->processData();
    if (!toEmit.empty()) return this->emit(toEmit);
    return {};
  }

  virtual OperatorMessage<T, V> processData() { return query(); }

  virtual OperatorMessage<T, V> processControl() { return query(); }

 private:
  V value;
  bool initialized;

  OperatorMessage<T, V> query() {
    OperatorMessage<T, V> outputMsgs;

    vector<string> in = this->getDataInputs();
    vector<string> cn = this->getControlInputs();

    string inputPort;
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw std::runtime_error(typeName() + ": " + "should have 1 input port, no more no less");

    string controlPort;
    if (cn.size() == 1)
      controlPort = cn.at(0);
    else
      throw std::runtime_error(typeName() + ": " + "should have 1 control port, no more no less");

    if (this->dataInputs.find(inputPort)->second.empty()) return {};
    if (this->controlInputs.find(controlPort)->second.empty()) return {};

    T queryTime = this->getControlInputMessage(controlPort, 0).time;

    if (queryTime < this->getDataInputMessage(inputPort, 0).time && !this->initialized) {
      Message<T, V> out;
      out.time = queryTime;
      out.value = this->value;
      PortMessage<T, V> v;
      v.push_back(out);
      outputMsgs.emplace("o1", v);
      this->controlInputs.find(controlPort)->second.pop_front();
      return outputMsgs;
    } else if (queryTime >= this->getDataInputMessage(inputPort, 0).time) {
      initialized = true;
      if (queryTime == this->getDataInputMessage(inputPort, 0).time) {
        Message<T, V> out;
        out.time = queryTime;
        out.value = this->getDataInputMessage(inputPort, 0).value;
        PortMessage<T, V> v;
        v.push_back(out);
        outputMsgs.emplace("o1", v);
        this->controlInputs.find(controlPort)->second.pop_front();
        return outputMsgs;
      }
      size_t index = 0;
      while (index < this->dataInputs.find(inputPort)->second.size() - 1) {
        if (queryTime > this->getDataInputMessage(inputPort, index).time &&
            queryTime < this->getDataInputMessage(inputPort, index + 1).time) {
          Message<T, V> out;
          out.time = queryTime;
          out.value = this->getDataInputMessage(inputPort, index).value;
          PortMessage<T, V> v;
          v.push_back(out);
          outputMsgs.emplace("o1", v);
          this->controlInputs.find(controlPort)->second.pop_front();
          return outputMsgs;
        } else if (queryTime == this->getDataInputMessage(inputPort, index + 1).time) {
          Message<T, V> out;
          out.time = queryTime;
          out.value = this->getDataInputMessage(inputPort, index + 1).value;
          PortMessage<T, V> v;
          v.push_back(out);
          outputMsgs.emplace("o1", v);
          this->controlInputs.find(controlPort)->second.pop_front();
          this->dataInputs.find(inputPort)->second.pop_front();
          return outputMsgs;
        } else {
          this->dataInputs.find(inputPort)->second.pop_front();
        }
      }
    } else if (queryTime < this->getDataInputMessage(inputPort, 0).time && this->initialized)
      throw std::runtime_error(typeName() + ": " + "PortMessage out of order detected");

    return {};
  }
};

}  // end namespace rtbot

#endif  // VARIABLE_H
