#ifndef COSINERESAMPLER_H
#define COSINERESAMPLER_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Buffer.h"

namespace rtbot {

struct CosineResampler : public Buffer<double> {
  static const int size = 2;

  unsigned int dt;
  std::uint64_t carryOver;

  CosineResampler() = default;

  CosineResampler(string const &id_, unsigned int dt_)
      : Buffer<double>(id_, CosineResampler::size), dt(dt_), carryOver(0) {}

  string typeName() const override { return "CosineResampler"; }

  map<string, std::vector<Message<>>> processData() override {
    std::vector<Message<>> toEmit;

    if ((std::int64_t)(at(1).time - at(0).time) <= 0) return {};

    int j = 1;

    while (at(1).time - at(0).time >= (j * dt) - carryOver) {
      Message<> out;
      double mu = ((j * dt) - carryOver) / (at(1).time - at(0).time);
      out.value = CosineResampler::cosineInterpolate(at(0).value, at(1).value, mu);
      out.time = at(0).time + ((j * dt) - carryOver);
      toEmit.push_back(out);
      j++;
    }

    carryOver = at(1).time - (at(0).time + (((j - 1) * dt) - carryOver));

    if (toEmit.size() > 0)
      return this->emit(toEmit);
    else
      return {};
  }

  /**
   * Calculations taken from http://paulbourke.net/miscellaneous/interpolation/
   */
  static double cosineInterpolate(double y1, double y2, double mu) {
    double mu2;
    mu2 = (1 - std::cos(mu * 3.141592653589)) / 2;
    return (y1 * (1 - mu2) + y2 * mu2);
  }
};

}  // namespace rtbot

#endif  // COSINERESAMPLER_H
