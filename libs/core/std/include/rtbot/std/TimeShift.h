#ifndef TIMESHIFT_H
#define TIMESHIFT_H

#include <math.h>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Adds a constant (dt * times) to each input message time.
 *   $$y(t_n)= x(t_n) + (dt * times)$$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   dt:
 *     type: integer
 *     default: 1
 *     minimum: 1
 *     description: The constant that defines the time grid.
 *   times:
 *     type: integer
 *     default: 1
 *     description: The multiplier to apppy.
 * required: ["id"]
 */
template <class T, class V>
struct TimeShift : public Operator<T, V> {
  TimeShift() = default;
  TimeShift(string const &id, T dt = 1, int times = 1) : Operator<T, V>(id) {
    this->addDataInput("i1", 1);
    this->addOutput("o1");
    this->dt = dt;
    this->times = times;
  }
  string typeName() const override { return "TimeShift"; }
  map<string, vector<Message<T, V>>> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);

    out.time = ((this->dt * this->times < 0) && (out.time > abs((double)(this->dt * this->times))) ||
                (this->dt * this->times > 0))
                   ? out.time + this->dt * this->times
                   : 0;
    vector<Message<T, V>> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }

  T getDT() const { return this->dt; }

  int getTimes() const { return this->times; }

 private:
  T dt;
  int times;
};

}  // namespace rtbot

#endif  // TIMESHIFT_H
