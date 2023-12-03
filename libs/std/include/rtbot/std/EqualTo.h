#ifndef EQUALTO_H
#define EQUALTO_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

template <class T, class V>
struct EqualTo : public FilterByValue<T, V> {
  EqualTo() = default;

  EqualTo(string const &id, V value) : FilterByValue<T, V>(id, [=](V number) { return number == value; }) {
    this->value = value;
  }
  string typeName() const override { return "EqualTo"; }

  V getValue() const { return this->value; }

 private:
  V value;
};

}  // namespace rtbot

#endif  // EQUALTO_H
