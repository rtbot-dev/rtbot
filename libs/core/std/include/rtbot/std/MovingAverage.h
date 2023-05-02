#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include "rtbot/Buffer.h"

namespace rtbot {

struct MovingAverage : public Buffer<double> {
  MovingAverage() = default;

  MovingAverage(string const& id_, int n_) : Buffer<double>(id_, n_) {}

  string typeName() const override { return "MovingAverage"; }

  map<string, std::vector<Message<>>> processData() override {
    Message<> out={at(size() / 2).time, 0};
    for (auto const& x : (*this))
      out.value += x.value / size();
    return emit(out);
  }
};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
