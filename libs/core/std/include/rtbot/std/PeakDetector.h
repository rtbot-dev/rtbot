#ifndef PEAKDETECTOR_H
#define PEAKDETECTOR_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct PeakDetector : Operator<T, V> {
  PeakDetector() = default;

  PeakDetector(string const& id, size_t n) : Operator<T, V>(id) {
    this->addDataInput("i1", n);
    this->addOutput("o1");
  }

  string typeName() const override { return "PeakDetector"; }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    size_t size = this->getDataInputSize(inputPort);
    size_t pos = size / 2;  // expected position of the max
    for (auto i = 0u; i < size; i++)
      if (this->getDataInputMessage(inputPort, pos).value < this->getDataInputMessage(inputPort, i).value) return {};
    return this->emit(this->getDataInputMessage(inputPort, pos));
  }
};

}  // end namespace rtbot

#endif  // PEAKDETECTOR_H
