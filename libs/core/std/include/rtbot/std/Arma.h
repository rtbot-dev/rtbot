
#ifndef ARMA_H
#define ARMA_H

#include "rtbot/std/AutoRegressive.h"
#include "rtbot/std/Composite.h"
#include "rtbot/std/MovingAverage.h"

namespace rtbot {

template <class T, class V>
struct ARMA : public Composite<T, V> {
  ARMA(string const& id_, vector<V> const& ar_, vector<V> const& ma_)
      : Composite<T, V>(id_, {std::make_unique<MovingAverage<T, V>>(id_ + "_ma", ma_),
                              std::make_unique<AutoRegressive<T, V>>(id_ + "_ar", ar_)}) {}

  ARMA(string const& id_, vector<V> const& ar_, int n_ma)
      : Composite<T, V>(id_, {std::make_unique<MovingAverage<T, V>>(id_ + "_ma", n_ma),
                              std::make_unique<AutoRegressive<T, V>>(id_ + "_ar", ar_)}) {}
};

}  // namespace rtbot

#endif  // ARMA_H