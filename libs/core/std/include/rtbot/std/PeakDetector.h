#ifndef PEAKDETECTOR_H
#define PEAKDETECTOR_H

#include "rtbot/Buffer.h"

namespace rtbot {

template <class T, class V>
struct PeakDetector : Buffer<T, V> {
  using Buffer<T, V>::Buffer;

  string typeName() const override { return "PeakDetector"; }

  map<string, std::vector<Message<T, V>>> processData() override {
    int pos = this->size() / 2;  // expected position of the max
    for (auto i = 0u; i < this->size(); i++)
      if (this->at(pos).value < this->at(i).value) return {};
    return this->emit(this->at(pos));
  }
};

}  // end namespace rtbot

#endif  // PEAKDETECTOR_H
