#ifndef MINUS_H
#define MINUS_H

#include "rtbot/Join.h"

namespace rtbot {

/**
 * @brief The Minus class as example of application of Join
 */
template <class T, class V>
struct Minus : public Join<T, V> {
  Minus() = default;
  Minus(string const &id, map<string, typename Operator<T, V>::InputPolicy> policies = {}) : Join<T, V>(id) {
    int eagerInputs = 0;
    int numPorts = 2;
    for (int i = 1; i <= numPorts; i++) {
      string inputPort = string("i") + to_string(i);
      if (policies.count(inputPort) > 0) {
        if (policies.find(inputPort)->second.isEager()) eagerInputs++;
        this->addDataInput(inputPort, 0, policies.find(inputPort)->second);
      } else
        this->addDataInput(inputPort, 0, {});
    }
    this->addOutput("o1");
    if (eagerInputs == numPorts)
      throw std::runtime_error(typeName() + ": at least one input port should be not eager.");
  }

  string typeName() const override { return "Minus"; }

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> m1 = this->getDataInputMessage("i2", 0);
    Message<T, V> m0 = this->getDataInputMessage("i1", 0);
    Message<T, V> out;
    out.time = (this->isDataInputEager("i1")) ? m1.time : m0.time;
    out.value = m0.value - m1.value;
    map<string, vector<Message<T, V>>> toEmit;
    vector<Message<T, V>> v;
    v.push_back(out);
    toEmit.emplace("o1", v);
    return toEmit;
  }
};

}  // namespace rtbot

#endif  // MINUS_H