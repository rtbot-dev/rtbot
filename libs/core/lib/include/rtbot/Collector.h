#ifndef COLLECTOR_H
#define COLLECTOR_H

#include "Operator.h"

namespace rtbot {

template <class T, class V>
struct Collector : public Operator<T, V> {
  Collector() = default;
  Collector(string const& id, size_t n = 1) : Operator<T, V>(id) { this->addDataInput("i1", n); }

  string typeName() const override { return "Collector"; }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override { return {}; }
};

}  // end namespace rtbot

#endif  // COLLECTOR_H
