#ifndef SCALE_H
#define SCALE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Scale : public Operator<T, V> {
  Scale() = default;
  Scale(string const &id, V factor) : Operator<T, V>(id) {
    this->addDataInput("i1", Scale::size);
    this->addOutput("o1");
    this->factor = factor;
  }
  string typeName() const override { return "Scale"; }
  map<string, vector<Message<T, V>>> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    map<string, vector<Message<T, V>>> outputMsgs;
    vector<Message<T, V>> toEmit;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    out.value = out.value * this->factor;
    toEmit.push_back(out);
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }

  V getFactor() const { return this->factor; }

 private:
  static const size_t size = 1;
  V factor;
};

}  // namespace rtbot

#endif  // SCALE_H
