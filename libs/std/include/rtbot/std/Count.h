#ifndef COUNT_H
#define COUNT_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Count : public Operator<T, V> {
  size_t count;
  Count() = default;
  Count(string const &id) : Operator<T, V>(id) {
    this->count = 0;
    this->addDataInput("i1", 1);
    this->addOutput("o1");
  }
  string typeName() const override { return "Count"; }
  PortPayload<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    PortPayload<T, V> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    this->count = this->count + 1;
    out.value = this->count;
    Messages<T, V> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }
};

}  // namespace rtbot

#endif  // COUNT_H
