#ifndef DIFFERENCE_H
#define DIFFERENCE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Difference : public Operator<T, V> {
  Difference() = default;

  Difference(string const &id, bool useOldestTime = true) : Operator<T, V>(id) {
    this->useOldestTime = useOldestTime;
    this->addDataInput("i1", Difference<T, V>::size);
    this->addOutput("o1");
  }

  string typeName() const override { return "Difference"; }

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> m1 = this->getDataInputMessage(inputPort, 1);
    Message<T, V> m0 = this->getDataInputMessage(inputPort, 0);
    Message<T, V> out;
    out.value = m1.value - m0.value;
    out.time = (this->useOldestTime) ? m1.time : m0.value;
    vector<Message<T, V>> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }

  bool getUseOldestTime() const { return this->useOldestTime; }

 private:
  static const int size = 2;
  bool useOldestTime;
};

}  // namespace rtbot

#endif  // DIFFERENCE_H
