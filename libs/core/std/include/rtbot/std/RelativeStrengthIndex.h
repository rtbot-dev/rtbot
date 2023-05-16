#ifndef RELATIVESTRENGTHINDEX_H
#define RELATIVESTRENGTHINDEX_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Buffer.h"

namespace rtbot {

template <class T, class V>
struct RelativeStrengthIndex : public Buffer<T, V> {
  RelativeStrengthIndex() = default;

  RelativeStrengthIndex(string const& id_, size_t n_) : Buffer<T, V>(id_, n_), initialized(false) {}

  string typeName() const override { return "RelativeStrengthIndex"; }

  map<string, std::vector<Message<T, V>>> processData() override {
    Message<T, V> out;
    size_t n = this->size();
    V diff, rs, rsi, gain, loss;

    if (!initialized) {
      averageGain = 0;
      averageLoss = 0;
      for (size_t i = 1; i < n; i++) {
        diff = this->at(i).value - this->at(i - 1).value;
        if (diff > 0)
          averageGain = averageGain + diff;
        else if (diff < 0)
          averageLoss = averageLoss - diff;
      }
      averageGain = averageGain / (n - 1);
      averageLoss = averageLoss / (n - 1);

      initialized = true;
    } else {
      diff = this->at(n - 1).value - this->at(n - 2).value;
      if (diff > 0) {
        gain = diff;
        loss = 0;
      } else if (diff < 0) {
        loss = -diff;
        gain = 0;
      } else {
        loss = 0;
        gain = 0;
      }
      averageGain = (prevAverageGain * (n - 2) + gain) / (n - 1);
      averageLoss = (prevAverageLoss * (n - 2) + loss) / (n - 1);
    }

    prevAverageGain = averageGain;
    prevAverageLoss = averageLoss;

    rs = averageGain / averageLoss;
    rsi = 100.0 - (100.0 / (1 + rs));

    out.value = rsi;
    out.time = this->back().time;

    return this->emit(out);
  }

 private:
  V averageGain;
  V averageLoss;
  V prevAverageGain;
  V prevAverageLoss;
  bool initialized = false;
};

}  // namespace rtbot

#endif  // RELATIVESTRENGTHINDEX_H
