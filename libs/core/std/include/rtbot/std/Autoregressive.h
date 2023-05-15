#ifndef AUTOREGRESSIVE_H
#define AUTOREGRESSIVE_H

#include "rtbot/ARBuffer.h"

namespace rtbot {

template <class T, class V>
struct AutoRegressive : public ARBuffer<T, V> {
  std::vector<V> coeff;

  AutoRegressive() = default;

  AutoRegressive(string const& id_, vector<V> const& coeff_) : ARBuffer<T, V>(id_, coeff_.size()), coeff(coeff_) {}

  string typeName() const override { return "AutoRegressive"; }

  Message<T, V> processData(Message<T, V> const& msg) override {
    Message<T, V> out = msg;
    for (auto i = 0; i < this->size(); i++) out.value += coeff[i] * this->at(this->size() - 1 - i).value;
    return out;
  }
};

}  // namespace rtbot

#endif  // AUTOREGRESSIVE_H
