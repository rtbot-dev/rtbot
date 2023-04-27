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
      for (auto j = 0u; j < this->at(i).value.size(); j++)
        if (at(pos).value[j] < at(i).value[j]) return {};
    std::vector<Message<>> msgs;
    msgs.push_back(at(pos));
    return emit(msgs);
  }
};

}  // end namespace rtbot

#endif  // PEAKDETECTOR_H
