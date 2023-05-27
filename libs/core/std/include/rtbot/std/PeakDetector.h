#ifndef PEAKDETECTOR_H
#define PEAKDETECTOR_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct PeakDetector : Operator<T, V> {
  PeakDetector() = default;

  PeakDetector(string const& id_, size_t n_) : Operator<T, V>(id_) {
    this->addInput("i1", n_);
    this->addOutput("o1");
  }

  string typeName() const override { return "PeakDetector"; }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    size_t size = this->getSize(inputPort);
    size_t pos = size / 2;  // expected position of the max
    for (auto i = 0u; i < size; i++)
      if (this->getMessage(inputPort, pos).value < this->getMessage(inputPort, i).value) return {};
    return this->emit(this->getMessage(inputPort, pos));
  }
};

}  // end namespace rtbot

#endif  // PEAKDETECTOR_H
