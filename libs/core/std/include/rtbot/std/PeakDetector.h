#ifndef PEAKDETECTOR_H
#define PEAKDETECTOR_H

#include "rtbot/Buffer.h"

namespace rtbot {

template <class V = double>
struct PeakDetector : Buffer<V> {
  using Buffer<V>::Buffer;

  string typeName() const override { return "PeakDetector"; }

  map<string, std::vector<Message<V>>> processData() override {
    int pos = this->size() / 2;  // expected position of the max
    for (auto i = 0u; i < this->size(); i++)
      if (this->at(pos).value < this->at(i).value) return {};
    return this->emit(this->at(pos));
  }
};

}  // end namespace rtbot

#endif  // PEAKDETECTOR_H
