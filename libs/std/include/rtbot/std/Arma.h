
#ifndef ARMA_H
#define ARMA_H

/*
#include "rtbot/std/AutoRegressive.h"
#include "rtbot/std/Composite.h"
#include "rtbot/std/MovingAverage.h"

namespace rtbot {

template <class T, class V>
struct ARMA : public Composite<T, V> {
  ARMA(string const& id, vector<V> const& ar, vector<V> const& ma)
      : Composite<T, V>(id, {std::make_unique<MovingAverage<T, V>>(id + "_ma", ma),
                             std::make_unique<AutoRegressive<T, V>>(id + "_ar", ar)}) {}

  ARMA(string const& id, vector<V> const& ar, int n_ma)
      : Composite<T, V>(id, {std::make_unique<MovingAverage<T, V>>(id + "_ma", n_ma),
                             std::make_unique<AutoRegressive<T, V>>(id + "_ar", ar)}) {}
};

}  // namespace rtbot
*/

#endif  // ARMA_H