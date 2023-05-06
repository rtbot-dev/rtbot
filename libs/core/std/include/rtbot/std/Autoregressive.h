#ifndef AUTOREGRESSIVE_H
#define AUTOREGRESSIVE_H

#include "MovingAverage.h"
#include "rtbot/Buffer.h"
#include "rtbot/Composite.h"

namespace rtbot {

template <class T, class V>
struct AutoRegressive : public AutoBuffer<T,V> {
  std::vector<V> coeff;

  AutoRegressive(string const& id_, vector<V> const& coeff_)
      : AutoBuffer<T,V>(id_, coeff_.size()), coeff(coeff_) {}

  Message<T,V> solve(Message<T,V> const& msg) const override {
    Message<T,V> out = msg;
    for (auto i = 0; i < n; i++)
      for (auto j = 0u; j < at(n - 1 - i).value.size(); j++) out.value[j] += coeff[i] * at(n - 1 - i).value[j];
    return out;
  }
};

template <class T, class V>
struct ARMA : public Composite<T,V> {
  ARMA(string const& id_, vector<V> const& ar_, vector<V> const& ma_)
      : Composite<T,V>(id_, {std::make_unique<MovingAverage<T,V>>(id_ + "_ma", ma_),
                                std::make_unique<AutoRegressive<T,V>>(id_ + "_ar", ar_)}) {}

  ARMA(string const& id_, vector<V> const& ar_, int n_ma)
      : Composite<T,V>(id_, {std::make_unique<MovingAverage<T,V>>(id_ + "_ma", n_ma),
                                std::make_unique<AutoRegressive<T,V>>(id_ + "_ar", ar_)}) {}
};

}  // namespace rtbot

#endif  // AUTOREGRESSIVE_H
