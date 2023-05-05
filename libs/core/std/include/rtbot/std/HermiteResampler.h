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

  std::uint64_t carryOver;

  HermiteResampler() = default;

  HermiteResampler(string const &id_, unsigned int dt_)
      : Buffer<T>(id_, HermiteResampler::size), dt(dt_), carryOver(0) {}

  string typeName() const override { return "HermiteResampler"; }

  map<string, std::vector<Message<T>>> processData() override {
    
    std::vector<Message<T>> toEmit;

    if (before.get() == nullptr) {
      toEmit = this->lookAt(0, 1);
      auto toAdd = this->lookAt(1, 2);
      toEmit.insert(toEmit.end(), toAdd.begin(), toAdd.end());

    } else {
      toEmit = this->lookAt(1, 2);
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

  std::vector<Message<T>> lookAt(int from, int to) {

    std::vector<Message<T>> toEmit;
    int j = 1;

    while (this->get(to).time - this->get(from).time >= (j * dt) - carryOver) {
      Message<T> out;
      double mu = ((j * dt) - carryOver) / (this->get(to).time - this->get(from).time);      
      out.value = HermiteResampler<T>::hermiteInterpolate(this->get(from - 1).value, this->get(from).value, this->get(to).value,this->get(to + 1).value, mu);
      out.time = this->get(from).time + ((j * dt) - carryOver);
      toEmit.push_back(out);
      j++;
    }

    carryOver = this->get(to).time - (this->get(from).time + (((j - 1) * dt) - carryOver));

    return toEmit;
  }
  
  /*
    To store a message(point) created artificially that will help us to interpolate on the interval [0,1]
    This message(point) will be exclusively used for interpolating on the interval [0,1] using the first 
    group of equidistant dts that fall into the interval [0,1].
  */
  std::unique_ptr<Message<T>> before = nullptr;

  /*
    This function decides whether data from the buffer should be used to suffice the request 
    or to artificially create a message(point) at the left of the interval [0,1] using the 
    least squares numeric method.
  */
  Message<T>& get(int index) {

    if (index >= 0) return this->at(index);
    else if (before.get() == nullptr)
    {
        std::vector<std::uint64_t> x;
        std::vector<T> y;
        std::uint64_t average = 0;
        int n = ((Buffer<T>*)this)->size();
        for(int i = 0; i < n; i++ )
        {
          x.push_back(this->at(i).time);
          y.push_back(this->at(i).value);
          average = average + this->at(i).time;
        }
        average = average / n;
        std::pair<T,T> pair = this->getLineLeastSquares(x,y);
        std::uint64_t time = this->at(0).time - (-1 * index * average);
        T value = pair.second * time + pair.first;
        before = std::make_unique<Message<T>>(Message(time,value));

    }
    return *(before.get());

  }

  std::pair<T,T> getLineLeastSquares(std::vector<uint64_t> x, std::vector<T> y)
  {
      T sumY = 0, sumXY = 0, n , m;
      std::uint64_t sumX = 0, sumX2 = 0;
      for(size_t i = 0; i < x.size(); i++) {
        sumX = sumX + x.at(i);
        sumY = sumY + y.at(i);
        sumXY = sumXY + x.at(i) * y.at(i);
        sumX2 = sumX2 + pow(x.at(i), 2);
      }

      std::uint64_t denominator = (x.size() * sumX2 - pow(sumX,2));

      if (denominator == 0) {

        m = (y.at(1) - y.at(0)) / (x.at(1) - x.at(0));
        n = y.at(0) - m * x.at(0);
      }
      else
      {
        n = (sumX2 * sumY - sumXY * sumX) / denominator;
        m = (x.size() * sumXY -sumX * sumY) / denominator;
      }

      return std::pair<T,T>(n,m);
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
