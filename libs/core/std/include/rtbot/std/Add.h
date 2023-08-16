#ifndef ADD_H
#define ADD_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Adds a constant specified value to each input message.
 *   $$y(t_n)= x(t_n) + C$$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   value:
 *     type: number
 *     description: The constant to add to the incoming messages.
 * required: ["id", "value"]
 */
template <class T, class V>
struct Add : public Operator<T, V> {
  Add() = default;
  Add(string const &id, V value) : Operator<T, V>(id) {
    this->addDataInput("i1", 1);
    this->addOutput("o1");
    this->value = value;
  }
  string typeName() const override { return "Add"; }
  map<string, vector<Message<T, V>>> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    out.value = out.value + this->value;
    vector<Message<T, V>> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }

  V getValue() const { return this->value; }

 private:
  V value;
};

}  // namespace rtbot

#endif  // ADD_H
