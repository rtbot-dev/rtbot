#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include "rtbot/Buffer.h"

namespace rtbot {

template <class T, class V>
struct MovingAverage : public Buffer<T, V> {
  MovingAverage() = default;

  MovingAverage(string const& id_, size_t n_) : Buffer<T, V>(id_, n_) {}

  string typeName() const override { return "MovingAverage"; }

  map<string, std::vector<Message<T, V>>> processData() override {
    std::vector<Message<T, V>> toEmit;
    Message<T, V> out;

    out.time = this->back().time;
    out.value = this->getSum() / this->size();

    toEmit.push_back(out);
    return this->emit(toEmit);
  }
};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
