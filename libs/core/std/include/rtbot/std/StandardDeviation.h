#ifndef STANDARDDEVIATION_H
#define STANDARDDEVIATION_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Buffer.h"

namespace rtbot {

template <class T = double>
struct StandardDeviation : public Buffer<T> {
  unsigned int iteration;

  StandardDeviation() = default;

  StandardDeviation(string const &id_, unsigned int n_) : Buffer<T>(id_, n_), iteration(0) {}

  string typeName() const override { return "StandardDeviation"; }

  map<string, std::vector<Message<T>>> processData() override {
    std::vector<Message<T>> toEmit;
    Message<T> out;

    T average;
    T std = 0;

    if (iteration == 0) {
      sum = 0;

      for (size_t j = 0; j < this->size(); j++) {
        sum = sum + this->at(j).value;
      }
      average = sum / this->size();
    } else {
      sum += this->back().value;
      average = sum / this->size();
    }

    iteration++;

    for (size_t j = 0; j < this->size(); j++) {
      std = std + pow(this->at(j).value - average, 2);
    }

    std = sqrt(std / (this->size() - 1));

    sum = sum - this->front().value;

    out.time = this->back().time;
    out.value = std;
    toEmit.push_back(out);

    return this->emit(toEmit);
  }

 private:
  T sum;
};

}  // namespace rtbot

#endif  // STANDARDDEVIATION_H
