#ifndef PEAKDETECTOR_H
#define PEAKDETECTOR_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct PeakDetector : Operator<T, V> {
  PeakDetector() = default;

  PeakDetector(string const& id, size_t n) : Operator<T, V>(id) {
    this->addDataInput("i1", n);
    this->addOutput("o1");
  }

  string typeName() const override { return "PeakDetector"; }

  PortPayload<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    PortPayload<T, V> outputMsgs;
    Messages<T, V> toEmit;
    size_t size = this->getDataInputSize(inputPort);
    size_t pos = size / 2;  // expected position of the max
    for (auto i = 0u; i < size; i++)
      if (this->getDataInputMessage(inputPort, pos).value < this->getDataInputMessage(inputPort, i).value) return {};

    toEmit.push_back(this->getDataInputMessage(inputPort, pos));
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }
};

}  // end namespace rtbot

#endif  // PEAKDETECTOR_H
