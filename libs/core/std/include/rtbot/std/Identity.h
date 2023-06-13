#ifndef IDENTITY_H
#define IDENTITY_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Identity : public Operator<T, V> {
  Identity() = default;

  Identity(string const &id, size_t delay = 0) : Operator<T, V>(id) {
    this->delay = delay;
    this->addInput("i1", this->delay + 1);
    this->addOutput("o1");
  }

  string typeName() const override { return "Identity"; }

  map<string, std::vector<Message<T, V>>> processData(string inputPort) override {
    Message<T, V> m = this->getFirstMessage(inputPort);
    return this->emit(m);
  }

  size_t getDelay() const { return this->delay; }

 private:
  size_t delay;
};

}  // namespace rtbot

#endif  // IDENTITY_H
