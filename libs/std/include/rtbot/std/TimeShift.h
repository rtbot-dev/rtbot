#ifndef TIMESHIFT_H
#define TIMESHIFT_H

#include <math.h>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

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
  PortPayload<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    PortPayload<T, V> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);

    out.time = ((this->dt * this->times < 0) && (out.time > abs((double)(this->dt * this->times))) ||
                (this->dt * this->times > 0))
                   ? out.time + this->dt * this->times
                   : 0;
    Messages<T, V> v;
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
