#ifndef STANDARDDEVIATION_H
#define STANDARDDEVIATION_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Buffer.h"

namespace rtbot {

template <class T = double>
struct StandardDeviation : public Buffer<T> {

  StandardDeviation() = default;

  StandardDeviation(string const &id_, unsigned int n_) : Buffer<T>(id_, n_) {}

  string typeName() const override { return "StandardDeviation"; }

  map<string, std::vector<Message<T>>> processData() override {
    std::vector<Message<T>> toEmit;
    Message<T> out;

    T average = this->sum / this->size();
    T std = 0;    

    for (size_t j = 0; j < this->size(); j++) {
      std = std + pow(this->at(j).value - average, 2);
    }

    std = sqrt(std / (this->size() - 1));
    
    out.time = this->back().time;
    out.value = std;
    toEmit.push_back(out);

    return this->emit(toEmit);
  }

};

}  // namespace rtbot

#endif  // STANDARDDEVIATION_H
