#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include "rtbot/Buffer.h"

namespace rtbot {

struct MovingAverage : public Buffer<double> {
  MovingAverage() = default;

  MovingAverage(string const& id_, int n_) : Buffer<double>(id_, n_) {}

  string typeName() const override { return "MovingAverage"; }

  map<string, std::vector<Message<>>> processData() override {
    Message<> out;
    out.time = at(size() / 2).time;
    out.value.assign(at(0).value.size(), 0);
    for (auto const& x : (*this))
      for (auto j = 0u; j < x.value.size(); j++) out.value[j] += x.value[j] / size();
    return emit(out);
  }
};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
