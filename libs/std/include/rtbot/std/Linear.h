#ifndef LINEAR_H
#define LINEAR_H

#include "rtbot/Join.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Linear : public Join<T, V> {
  Linear() = default;
  Linear(string const& id, vector<V> const& coeff) : Join<T, V>(id) {
    if (coeff.size() < 2) throw runtime_error(typeName() + ": number of ports have to be greater than or equal 2");
    this->coeff = coeff;

    for (size_t i = 1; i <= this->coeff.size(); i++) {
      string inputPort = string("i") + to_string(i);
      this->addDataInput(inputPort, 0);
    }
    this->addOutput("o1");
  }

  string typeName() const override { return "Linear"; }

  /*
    map<outputPort, Messages<T, V>>
  */
  PortPayload<T, V> processData() override {
    PortPayload<T, V> outputMsgs;
    int i = 0;
    Message<T, V> out;
    out.value = 0;
    out.time = 0;
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      out.value = out.value + it->second.front().value * this->coeff.at(i);
      out.time = it->second.front().time;
      i++;
    }

    Messages<T, V> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);

    return outputMsgs;
  }

  vector<V> getCoefficients() const { return this->coeff; }

 private:
  vector<V> coeff;
};

}  // namespace rtbot

#endif  // LINEAR_H