#ifndef HERMITERESAMPLER_H
#define HERMITERESAMPLER_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct HermiteResampler : public Operator<T, V> {
  static const size_t size = 4;

  T dt;
  T carryOver;

  HermiteResampler() = default;

  HermiteResampler(string const& id, T dt) : Operator<T, V>(id) {
    this->dt = dt;
    this->carryOver = 0;
    this->addDataInput("i1", HermiteResampler::size);
    this->addOutput("o1");
  }

  string typeName() const override { return "HermiteResampler"; }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    std::vector<Message<T, V>> toEmit;

    if (before.get() == nullptr) {
      toEmit = this->lookAt(0, 1, inputPort);
      auto toAdd = this->lookAt(1, 2, inputPort);
      toEmit.insert(toEmit.end(), toAdd.begin(), toAdd.end());

    } else {
      toEmit = this->lookAt(1, 2, inputPort);
    }

    if (toEmit.size() > 0)
      return this->emit(toEmit);
    else
      return {};
  }

 private:
  /*
    This function will conveniently execute the Hermite Interpolation centered on the interval
    [0,1] or on the interval [1,2]. For the case of the interval [0,1] it artificially creates a
    message(point) at the left of the first one at position [-1] so that we can interpolate using
    the four points required for the Hermite Interpolation execution.
  */

  std::vector<Message<T, V>> lookAt(int from, int to, string inputPort) {
    std::vector<Message<T, V>> toEmit;
    int j = 1;

    while (this->get(to, inputPort).time - this->get(from, inputPort).time >= (j * dt) - carryOver) {
      Message<T, V> out;
      V mu = (V)((j * dt) - carryOver) / (V)(this->get(to, inputPort).time - this->get(from, inputPort).time);
      out.value = HermiteResampler<T, V>::hermiteInterpolate(
          this->get(from - 1, inputPort).value, this->get(from, inputPort).value, this->get(to, inputPort).value,
          this->get(to + 1, inputPort).value, mu);
      out.time = this->get(from, inputPort).time + ((j * dt) - carryOver);
      toEmit.push_back(out);
      j++;
    }

    carryOver = this->get(to, inputPort).time - (this->get(from, inputPort).time + (((j - 1) * dt) - carryOver));

    return toEmit;
  }

  /*
    To store a message(point) created artificially that will help us to interpolate on the interval [0,1]
    This message(point) will be exclusively used for interpolating on the interval [0,1] using the first
    group of equidistant dts that fall into the interval [0,1].
  */
  std::unique_ptr<Message<T, V>> before = nullptr;

  /*
    This function decides whether data from the buffer should be used to suffice the request
    or to artificially create a message(point) at the left of the interval [0,1] using the
    least squares numeric method.
  */
  Message<T, V> get(int index, string inputPort) {
    if (index >= 0)
      return this->getDataInputMessage(inputPort, index);
    else if (before.get() == nullptr) {
      std::vector<T> x;
      std::vector<V> y;
      V average = 0;
      size_t n = this->getDataInputSize(inputPort);
      for (int i = 0; i < n; i++) {
        x.push_back(this->getDataInputMessage(inputPort, i).time);
        y.push_back(this->getDataInputMessage(inputPort, i).value);
      }
      for (size_t i = 1; i < n; i++) {
        average =
            average + (this->getDataInputMessage(inputPort, i).time - this->getDataInputMessage(inputPort, i - 1).time);
      }
      average = average / (n - 1);
      std::pair<V, V> pair = this->getLineLeastSquares(x, y);
      T time = this->getDataInputMessage(inputPort, 0).time + (index * ((T)average));
      V value = pair.second * time + pair.first;
      before = std::make_unique<Message<T, V>>(Message<T, V>(time, value));
    }
    return *(before.get());
  }

  /*
    Calculation taken from http://www.ccas.ru/mmes/educat/lab04/02/least-squares.c
  */
  /*
      x is the vector of the x-axis
      y is the vector of the y-axis
      the coordinates of a point are (x_i;y_i)
  */
  std::pair<V, V> getLineLeastSquares(std::vector<T> x, std::vector<V> y) {
    V sumY = 0, sumXY = 0, n, m;
    T sumX = 0, sumX2 = 0;
    for (size_t i = 0; i < x.size(); i++) {
      sumX = sumX + x.at(i);
      sumY = sumY + y.at(i);
      sumXY = sumXY + x.at(i) * y.at(i);
      sumX2 = sumX2 + pow(x.at(i), 2);
    }

    T denominator = (x.size() * sumX2 - pow(sumX, 2));

    if (denominator == 0) {
      m = (V)(y.at(1) - y.at(0)) / (V)(x.at(1) - x.at(0));
      n = (V)(y.at(0) - m * x.at(0));
    } else {
      n = (V)(sumX2 * sumY - sumXY * sumX) / denominator;
      m = (V)(x.size() * sumXY - sumX * sumY) / denominator;
    }

    return std::pair<V, V>(n, m);
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
  static V hermiteInterpolate(V y0, V y1, V y2, V y3, V mu, V tension = 0, V bias = 0) {
    V m0, m1, mu2, mu3;
    V a0, a1, a2, a3;

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
