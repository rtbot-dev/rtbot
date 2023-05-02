#ifndef PEAKDETECTOR_H
#define PEAKDETECTOR_H

#include "rtbot/Buffer.h"

namespace rtbot {

struct PeakDetector : Buffer<double> {
  using Buffer<double>::Buffer;

  string typeName() const override { return "PeakDetector"; }

  map<string, std::vector<Message<>>> processData() override {
    int pos = size() / 2;  // expected position of the max
    for (auto i = 0u; i < size(); i++)
        if (at(pos).value < at(i).value) return {};
    return emit(at(pos));
  }
};

}  // end namespace rtbot

#endif  // PEAKDETECTOR_H
