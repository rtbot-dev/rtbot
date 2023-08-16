#ifndef SCALE_H
#define SCALE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Emits messages with values multiplied by the number specified:
 *   $$y(t_n)=factor x(t_n)$$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   value:
 *     type: number
 *     description: The factor to use to scale the messages.
 * required: ["id", "value"]
 */
template <class T, class V>
struct Scale : public Operator<T, V> {
  Scale() = default;
  Scale(string const &id, V value) : Operator<T, V>(id) {
    this->addDataInput("i1", 1);
    this->addOutput("o1");
    this->value = value;
  }
  string typeName() const override { return "Scale"; }
  map<string, vector<Message<T, V>>> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    map<string, vector<Message<T, V>>> outputMsgs;
    vector<Message<T, V>> toEmit;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    out.value = out.value * this->value;
    toEmit.push_back(out);
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }

  V getValue() const { return this->value; }

 private:
  V value;
};

}  // namespace rtbot

#endif  // SCALE_H
