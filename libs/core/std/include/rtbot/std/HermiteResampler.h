#ifndef HERMITERESAMPLER_H
#define HERMITERESAMPLER_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/std/CosineResampler.h"

namespace rtbot {

struct HermiteResampler : public Buffer<double> {
  
  static const int size = 4;

  unsigned int dt;

  unsigned int iteration;

  std::uint64_t carryOver;

  HermiteResampler() = default;

  HermiteResampler(string const &id_, unsigned int dt_)
      : Buffer<double>(id_, HermiteResampler::size), dt(dt_), iteration(0), carryOver(0) {}

  string typeName() const override { return "HermiteResampler"; }

  map<string, std::vector<Message<>>> processData() override {
    std::vector<Message<>> toEmit;

    if ((std::int64_t)(at(1).time - at(0).time) <= 0 || (std::int64_t)(at(2).time - at(1).time) <= 0 ||
        (std::int64_t)(at(3).time - at(2).time <= 0))
      return {};

    if (iteration == 0) {
      toEmit = lookAt(0, 1);
      auto toAdd = lookAt(1, 2);
      toEmit.insert(toEmit.end(), toAdd.begin(), toAdd.end());

    } else {
      toEmit = lookAt(1, 2);
    }

    iteration++;

    if (toEmit.size() > 0)
      return this->emit(toEmit);
    else
      return {};
  }

 private:
  /*
    This function will conveniently select what type of resampling will be used depending on
    the indexes of the points where the dt will fall into. cosineInterpolate will only be call
    for those dts in the first iteration that fall into point 0 and 1, hermiteInterpolate will
    be use for all other cases regarless the iteration.
  */

  std::vector<Message<>> lookAt(int from, int to) {
    std::vector<Message<>> toEmit;
    int j = 1;

    while (at(to).time - at(from).time >= (j * dt) - carryOver) {
      Message<> out;
      double mu = ((j * dt) - carryOver) / (at(to).time - at(from).time);
      if (from == 0 && to == 1)
        out.value = CosineResampler::cosineInterpolate(at(from).value, at(to).value, mu);
      else if (from == 1 && to == 2)
        out.value = HermiteResampler::hermiteInterpolate(at(from - 1).value, at(from).value, at(to).value,
                                                         at(to + 1).value, mu);

      out.time = at(from).time + ((j * dt) - carryOver);
      toEmit.push_back(out);
      j++;
    }

    carryOver = at(to).time - (at(from).time + (((j - 1) * dt) - carryOver));

    return toEmit;
  }

  /*
      Calculations taken from http://paulbourke.net/miscellaneous/interpolation/
  */
  /*
      Tension: 1 is high, 0 normal, -1 is low
      Bias: 0 is even,
              positive is towards first segment,
              negative towards the other
  */
  static double hermiteInterpolate(double y0, double y1, double y2, double y3, double mu, double tension = 0,
                                   double bias = 0) {
    double m0, m1, mu2, mu3;
    double a0, a1, a2, a3;

    mu2 = mu * mu;
    mu3 = mu2 * mu;
    m0 = (y1 - y0) * (1 + bias) * (1 - tension) / 2;
    m0 += (y2 - y1) * (1 - bias) * (1 - tension) / 2;
    m1 = (y2 - y1) * (1 + bias) * (1 - tension) / 2;
    m1 += (y3 - y2) * (1 - bias) * (1 - tension) / 2;
    a0 = 2 * mu3 - 3 * mu2 + 1;
    a1 = mu3 - 2 * mu2 + mu;
    a2 = mu3 - mu2;
    a3 = -2 * mu3 + 3 * mu2;

    return (a0 * y1 + a1 * m0 + a2 * m1 + a3 * y2);
  }
};

}  // namespace rtbot

#endif  // HERMITERESAMPLER_H
