#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct MovingAverage : public Operator<T, V> {
  MovingAverage() = default;

  MovingAverage(string const& id, size_t n) : Operator<T, V>(id) {
    this->addInput("i1", n);
    this->addOutput("o1");
  }

  string typeName() const override { return "MovingAverage"; }

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    vector<Message<T, V>> toEmit;
    Message<T, V> out;

    out.time = this->getLastMessage(inputPort).time;
    out.value = this->getSum(inputPort) / this->getSize(inputPort);

    toEmit.push_back(out);
    return this->emit(toEmit);
  }
};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
