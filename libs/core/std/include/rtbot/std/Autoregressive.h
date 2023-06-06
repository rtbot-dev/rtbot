#ifndef AUTOREGRESSIVE_H
#define AUTOREGRESSIVE_H

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct AutoRegressive : public Operator<T, V> {
  vector<V> coeff;

  AutoRegressive() = default;

  AutoRegressive(string const& id_, vector<V> const& coeff_) : Operator<T, V>(id_), coeff(coeff_) {
    this->addInput("i1", coeff_.size());
    this->addOutput("o1");
  }

  string typeName() const override { return "AutoRegressive"; }

  map<string, std::vector<Message<T, V>>> receive(Message<T, V> const& msg, string inputPort = "") override {
    if (inputPort.empty()) {
      auto in = this->getInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (this->inputs.count(inputPort) > 0) {
      size_t n = this->getMaxSize(inputPort);
      while (this->getSize(inputPort) < n)
        this->inputs.find(inputPort)->second.push_back(Message<T, V>(0, 0));  // boundary conditions=0
      Message<T, V> out = msg;
      for (auto i = 0; i < n; i++) out.value += coeff[i] * this->getMessage(inputPort, n - 1 - i).value;
      this->inputs.find(inputPort)->second.pop_front();
      this->inputs.find(inputPort)->second.push_back(out);
      return this->emit(out);
    } else
      throw std::runtime_error(typeName() + ": " + inputPort + " : refers to a non existing input port");
    return {};
  }

  /*Nothing to do here*/
  map<string, vector<Message<T, V>>> processData(string inputPort) override {}
};

}  // namespace rtbot

#endif  // AUTOREGRESSIVE_H
