#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct MovingAverage : public Operator<T, V> {
  MovingAverage() = default;

  MovingAverage(string const& id, size_t n) : Operator<T, V>(id) {
    this->addDataInput("i1", n);
    this->addOutput("o1");
  }

  string typeName() const override { return "MovingAverage"; }

  OperatorMessage<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    OperatorMessage<T, V> outputMsgs;
    PortMessage<T, V> toEmit;
    Message<T, V> out;

    out.time = this->getDataInputLastMessage(inputPort).time;
    out.value = this->getDataInputSum(inputPort) / this->getDataInputSize(inputPort);

    toEmit.push_back(out);
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }
};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
