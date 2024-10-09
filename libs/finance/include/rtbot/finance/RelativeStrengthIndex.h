#ifndef RELATIVESTRENGTHINDEX_H
#define RELATIVESTRENGTHINDEX_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct RelativeStrengthIndex : public Operator<T, V> {
  RelativeStrengthIndex() = default;

  RelativeStrengthIndex(string const& id, size_t n) : Operator<T, V>(id), initialized(false) {
    this->addDataInput("i1", n + 1);
    this->addOutput("o1");
  }

  string typeName() const override { return "RelativeStrengthIndex"; }

  PortPayload<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    Message<T, V> out;
    size_t n = this->getDataInputSize(inputPort);
    V diff, rs, rsi, gain, loss;

    if (!initialized) {
      averageGain = 0;
      averageLoss = 0;
      for (size_t i = 1; i < n; i++) {
        diff = this->getDataInputMessage(inputPort, i).value - this->getDataInputMessage(inputPort, i - 1).value;
        if (diff > 0)
          averageGain = averageGain + diff;
        else if (diff < 0)
          averageLoss = averageLoss - diff;
      }
      averageGain = averageGain / (n - 1);
      averageLoss = averageLoss / (n - 1);

      initialized = true;
    } else {
      diff = this->getDataInputMessage(inputPort, n - 1).value - this->getDataInputMessage(inputPort, n - 2).value;
      if (diff > 0) {
        gain = diff;
        loss = 0;
      } else if (diff < 0) {
        loss = -diff;
        gain = 0;
      } else {
        loss = 0;
        gain = 0;
      }
      averageGain = (prevAverageGain * (n - 2) + gain) / (n - 1);
      averageLoss = (prevAverageLoss * (n - 2) + loss) / (n - 1);
    }

    prevAverageGain = averageGain;
    prevAverageLoss = averageLoss;

    if (averageLoss > 0) {
      rs = averageGain / averageLoss;

      rsi = 100.0 - (100.0 / (1 + rs));
    } else
      rsi = 100.0;
    out.value = rsi;

    out.time = this->getDataInputLastMessage(inputPort).time;

    PortPayload<T, V> outputMsgs;
    Messages<T, V> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }

 private:
  V averageGain;
  V averageLoss;
  V prevAverageGain;
  V prevAverageLoss;
  bool initialized = false;
};

}  // namespace rtbot

#endif  // RELATIVESTRENGTHINDEX_H
