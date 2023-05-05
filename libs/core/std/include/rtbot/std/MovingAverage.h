#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include "rtbot/Buffer.h"

namespace rtbot {

template <class V = double>
struct MovingAverage : public Buffer<V> {
  MovingAverage() = default;

  MovingAverage(string const& id_, int n_) : Buffer<V>(id_, n_) {}

  string typeName() const override { return "MovingAverage"; }

  map<string, std::vector<Message<V>>> processData() override {
    std::vector<Message<V>> toEmit;
    Message<V> out;

    out.time = this->back().time;
    out.value = this->sum / this->size();

    toEmit.push_back(out);
    return this->emit(toEmit);
  }
};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
