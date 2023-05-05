#ifndef COSINERESAMPLER_H
#define COSINERESAMPLER_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Buffer.h"

namespace rtbot {

template <class V = double>
struct CosineResampler : public Buffer<V> {
  static const int size = 2;
  unsigned int dt;
  std::uint64_t carryOver;

  CosineResampler() = default;

  CosineResampler(string const &id_, unsigned int dt_) : Buffer<V>(id_, CosineResampler::size), dt(dt_), carryOver(0) {}

  string typeName() const override { return "CosineResampler"; }

  map<string, std::vector<Message<V>>> processData() override {
    std::vector<Message<V>> toEmit;

    int j = 1;

    while (this->at(1).time - this->at(0).time >= (j * dt) - carryOver) {
      Message<V> out;
      double mu = ((j * dt) - carryOver) / (this->at(1).time - this->at(0).time);
      out.value = CosineResampler<V>::cosineInterpolate(this->at(0).value, this->at(1).value, mu);
      out.time = this->at(0).time + ((j * dt) - carryOver);
      toEmit.push_back(out);
      j++;
    }

    carryOver = this->at(1).time - (this->at(0).time + (((j - 1) * dt) - carryOver));

    if (toEmit.size() > 0)
      return this->emit(toEmit);
    else
      return {};
  }

 private:
  /**
   * Calculations taken from http://paulbourke.net/miscellaneous/interpolation/
   */
  static V cosineInterpolate(V y1, V y2, double mu) {
    double mu2;
    mu2 = (1 - std::cos(mu * 3.141592653589)) / 2;
    return (y1 * (1 - mu2) + y2 * mu2);
  }
};

}  // namespace rtbot

#endif  // COSINERESAMPLER_H
