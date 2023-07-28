#ifndef POWER_H
#define POWER_H

#include <cmath>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Emits messages with values equal to the power specified:
 *   $$y(t_n)=x(t_n)^{power}$$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   power:
 *     type: number
 *     description: The exponent.
 * required: ["id", "power"]
 */
template <class T, class V>
struct Power : public Operator<T, V> {
  Power() = default;
  Power(string const &id, V power) : Operator<T, V>(id) {
    this->addDataInput("i1", Power::size);
    this->addOutput("o1");
    this->power = power;
  }
  string typeName() const override { return "Power"; }
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
    out.value = pow(out.value, this->power);
    toEmit.push_back(out);
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }

  V getPower() const { return this->power; }

 private:
  static const size_t size = 1;
  V power;
};

}  // namespace rtbot

#endif  // POWER_H
