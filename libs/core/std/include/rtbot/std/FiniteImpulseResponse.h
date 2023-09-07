#ifndef FINITEIMPULSERESPONSE_H
#define FINITEIMPULSERESPONSE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct FiniteImpulseResponse : public Operator<T, V> {
  FiniteImpulseResponse() = default;

  FiniteImpulseResponse(string const& id, vector<V> coeff) : Operator<T, V>(id) {
    this->addDataInput("i1", coeff.size());
    this->addOutput("o1");
    this->coeff = coeff;
  }

  string typeName() const override { return "FiniteImpulseResponse"; }

  map<string, vector<Message<T, V>>> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    map<string, vector<Message<T, V>>> outputMsgs;
    vector<Message<T, V>> toEmit;
    Message<T, V> out;

    out.time = this->getDataInputLastMessage(inputPort).time;
    out.value = 0;

    size_t size = this->dataInputs.find(inputPort)->second.size();

    for (int i = 0; i < size; i++) {
      out.value = out.value + this->dataInputs.find(inputPort)->second.at((size - 1) - i).value * this->coeff.at(i);
    }

    toEmit.push_back(out);
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }

  vector<V> getCoefficients() const { return this->coeff; }

 private:
  vector<V> coeff;
};

}  // namespace rtbot

#endif  // FINITEIMPULSERESPONSE_H
