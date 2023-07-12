#ifndef IDENTITY_H
#define IDENTITY_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Identity : public Operator<T, V> {
  Identity() = default;

  Identity(string const &id, size_t delay = 0) : Operator<T, V>(id) {
    this->delay = delay;
    this->addDataInput("i1", this->delay + 1);
    this->addOutput("o1");
  }

  string typeName() const override { return "Identity"; }

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> out = this->getDataInputFirstMessage(inputPort);
    vector<Message<T, V>> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }

  size_t getDelay() const { return this->delay; }

 private:
  size_t delay;
};

}  // namespace rtbot

#endif  // IDENTITY_H
