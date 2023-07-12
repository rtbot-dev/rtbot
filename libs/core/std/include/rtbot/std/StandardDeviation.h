#ifndef STANDARDDEVIATION_H
#define STANDARDDEVIATION_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct StandardDeviation : public Operator<T, V> {
  StandardDeviation() = default;

  StandardDeviation(string const &id, size_t n) : Operator<T, V>(id) {
    this->addDataInput("i1", n);
    this->addOutput("o1");
  }

  string typeName() const override { return "StandardDeviation"; }

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    vector<Message<T, V>> toEmit;
    Message<T, V> out;
    size_t size = this->getDataInputSize(inputPort);

    V average = this->getDataInputSum(inputPort) / size;
    V std = 0;

    for (size_t j = 0; j < size; j++) {
      std = std + pow(this->getDataInputMessage(inputPort, j).value - average, 2);
    }

    std = sqrt(std / (size - 1));

    out.time = this->getDataInputLastMessage(inputPort).time;
    out.value = std;
    toEmit.push_back(out);
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }
};

}  // namespace rtbot

#endif  // STANDARDDEVIATION_H
