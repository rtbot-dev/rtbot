#ifndef VARIABLE_H
#define VARIABLE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   A variable is a special operator designed to store stateful computations.
 *   It has one data input port and one control port. Messages received through
 *   the data input port are considered as definitions for the values of the variable
 *   from the time of the message up to the next different message time.
 *
 *   Messages received through the control port will trigger the emission of the value
 *   of the variable, according with the time present in the control message, through
 *   the output port.
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   defaultValue:
 *     type: number
 *     description: The default value of the variable
 *     default: 0
 * required: ["id"]
 */
template <class T, class V>
class Variable : public Operator<T, V> {
 public:
  Variable() = default;
  Variable(string const &id, V defaultValue = 0) : Operator<T, V>(id) {
    this->defaultValue = defaultValue;
    this->initialized = false;
    this->addDataInput("i1");
    this->addControlInput("c1", 1);
    this->addOutput("o1");
  }
  virtual ~Variable() = default;

  virtual string typeName() const override { return "Variable"; }

  V getDefaultValue() const { return this->defaultValue; }

  map<string, map<string, vector<Message<T, V>>>> executeData() override {
    auto toEmit = this->processData();
    if (!toEmit.empty()) return this->emit(toEmit);
    return {};
  }

  /*
    map<outputPort, vector<Message<T, V>>>
  */
  virtual map<string, vector<Message<T, V>>> processData() { return query(); }

  /*
      map<outputPort, vector<Message<T, V>>>
  */
  virtual map<string, vector<Message<T, V>>> processControl() { return query(); }

 private:
  V defaultValue;
  bool initialized;
  /*
      map<outputPort, vector<Message<T, V>>>
  */
  map<string, vector<Message<T, V>>> query() {
    map<string, vector<Message<T, V>>> outputMsgs;

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
      out.value = this->defaultValue;
      vector<Message<T, V>> v;
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
        vector<Message<T, V>> v;
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
          vector<Message<T, V>> v;
          v.push_back(out);
          outputMsgs.emplace("o1", v);
          this->controlInputs.find(controlPort)->second.pop_front();
          return outputMsgs;
        } else if (queryTime == this->getDataInputMessage(inputPort, index + 1).time) {
          Message<T, V> out;
          out.time = queryTime;
          out.value = this->getDataInputMessage(inputPort, index + 1).value;
          vector<Message<T, V>> v;
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
      throw std::runtime_error(typeName() + ": " + "Messages out of order detected");

    return {};
  }
};

}  // end namespace rtbot

#endif  // VARIABLE_H
