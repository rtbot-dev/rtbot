#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include "rtbot/Buffer.h"

namespace rtbot {

template <class T = double>
struct MovingAverage : public Buffer<T> {
  MovingAverage() = default;

  MovingAverage(string const& id_, int n_) : Buffer<T>(id_, n_) {}

  string typeName() const override { return "MovingAverage"; }

  map<string, std::vector<Message<T>>> processData() override {
    std::vector<Message<T>> toEmit;
    Message<T> out;

    out.time = this->back().time;
    out.value = this->sum / this->size();

    toEmit.push_back(out);
    return this->emit(toEmit);
  }
};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
