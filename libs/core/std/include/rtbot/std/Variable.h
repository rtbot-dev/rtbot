#ifndef VARIABLE_H
#define VARIABLE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
class Variable : public Operator<T, V> {
 public:
  Variable() = default;
  Variable(string const &id) : Operator<T, V>(id) {
    this->addDataInput("i1", 1);
    this->addControlInput("c1", 1);
    this->addOutput("o1");
  }
  virtual ~Variable() = default;

  virtual string typeName() const override { return "Variable"; }

  /*
    map<outputPort, vector<Message<T, V>>>
  */
  virtual map<string, vector<Message<T, V>>> processData() { return join(); }

  /*
      map<outputPort, vector<Message<T, V>>>
  */
  virtual map<string, vector<Message<T, V>>> processControl() { return join(); }

 private:
  map<string, string> controlMap;

  /*
      map<outputPort, vector<Message<T, V>>>
  */
  map<string, vector<Message<T, V>>> join() {
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

    if (this->controlInputs.find(controlPort)->second.front().time ==
        this->dataInputs.find(inputPort)->second.front().time) {
      vector<Message<T, V>> v;
      v.push_back(this->dataInputs.find(inputPort)->second.front());
      outputMsgs.emplace("o1", v);
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() -
                                                      this->dataInputs.find(inputPort)->second.front().value);
      this->dataInputs.find(inputPort)->second.pop_front();
      this->controlInputs.find(controlPort)
          ->second.setSum(this->controlInputs.find(controlPort)->second.getSum() -
                          this->controlInputs.find(controlPort)->second.front().value);
      this->controlInputs.find(controlPort)->second.pop_front();
    }

    if (outputMsgs.size() > 0) return outputMsgs;

    return {};
  }
};

}  // end namespace rtbot

#endif  // VARIABLE_H
