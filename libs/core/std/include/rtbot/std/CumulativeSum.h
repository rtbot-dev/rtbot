#ifndef CUMULATIVESUM_H
#define CUMULATIVESUM_H

#include <cstdint>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct CumulativeSum : public Operator<T, V> {
  CumulativeSum() = default;
  CumulativeSum(string const &id) : Operator<T, V>(id) {
    this->accumulated = 0;
    this->addDataInput("i1", CumulativeSum::size);
    this->addOutput("o1");
  }
  string typeName() const override { return "CumulativeSum"; }
  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    this->accumulated = this->accumulated + out.value;
    out.value = this->accumulated;
    return this->emit(out);
  }

 private:
  static const size_t size = 1;
  V accumulated;
};

}  // namespace rtbot

#endif  // CUMULATIVESUM_H
