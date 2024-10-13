#ifndef FILTERBYVALUE_H
#define FILTERBYVALUE_H

#include <functional>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct FilterByValue : public Operator<T, V> {
  function<bool(V)> filter;

  FilterByValue() = default;
  FilterByValue(string const& id, function<bool(V)> filter) : Operator<T, V>(id) {
    this->filter = filter;
    this->addDataInput("i1", 1);
    this->addOutput("o1");
  }

  OperatorMessage<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(this->typeName() + " : more than 1 input port found");
    OperatorMessage<T, V> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    if (filter(out.value)) {
      PortMessage<T, V> v;
      v.push_back(out);
      outputMsgs.emplace("o1", v);
      return outputMsgs;
    }
    return {};
  }
};
}  // namespace rtbot

#endif  // FILTERBYVALUE_H
