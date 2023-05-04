#ifndef HERMITERESAMPLER_H
#define HERMITERESAMPLER_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Buffer.h"
#include "rtbot/std/CosineResampler.h"

namespace rtbot {

template <class T = double>
struct HermiteResampler : public Buffer<T> {
  static const int size = 4;

  unsigned int dt;

  unsigned int iteration;

  std::uint64_t carryOver;

  HermiteResampler() = default;

  HermiteResampler(string const &id_, unsigned int dt_)
      : Buffer<T>(id_, HermiteResampler::size), dt(dt_), iteration(0), carryOver(0) {}

  string typeName() const override { return "HermiteResampler"; }

  map<string, std::vector<Message<T>>> processData() override {
    std::vector<Message<T>> toEmit;

    if ((std::int64_t)(this->at(1).time - this->at(0).time) <= 0 || (std::int64_t)(this->at(2).time - this->at(1).time) <= 0 ||
        (std::int64_t)(this->at(3).time - this->at(2).time <= 0))
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

  std::vector<Message<T>> lookAt(int from, int to) {
    std::vector<Message<T>> toEmit;
    int j = 1;

    while (this->at(to).time - this->at(from).time >= (j * dt) - carryOver) {
      Message<> out;
      double mu = ((j * dt) - carryOver) / (this->at(to).time - this->at(from).time);
      if (from == 0 && to == 1)
        out.value = CosineResampler<T>::cosineInterpolate(this->at(from).value, this->at(to).value, mu);
      else if (from == 1 && to == 2)
        out.value = HermiteResampler<T>::hermiteInterpolate(this->at(from - 1).value, this->at(from).value, this->at(to).value,
                                                         this->at(to + 1).value, mu);

      out.time = this->at(from).time + ((j * dt) - carryOver);
      toEmit.push_back(out);
      j++;
    }

    carryOver = this->at(to).time - (this->at(from).time + (((j - 1) * dt) - carryOver));

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
  static T hermiteInterpolate(T y0, T y1, T y2, T y3, double mu, double tension = 0,
                                   double bias = 0) {
    double m0, m1, mu2, mu3;
    T a0, a1, a2, a3;

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
