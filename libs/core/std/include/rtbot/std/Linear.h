#ifndef LINEAR_H
#define LINEAR_H

#include "rtbot/Join.h"

namespace rtbot {

/**
 * @brief The Difference class as example of application of Join
 */
template <class T, class V>
struct Linear : public Join<T, V> {
  vector<V> coeff;

  Linear() = default;
  Linear(string const& id_, vector<V> const& coeff_, map<string, typename Operator<T, V>::InputPolicy> _policies = {})
      : coeff(coeff_) {
    if (coeff.size() < 2) throw std::runtime_error(typeName() + ": number of ports have to be greater than or equal 2");
    this->id = id_;
    int eagerInputs = 0;
    for (size_t i = 1; i <= coeff.size(); i++) {
      string inputPort = string("i") + to_string(i);
      if (_policies.count(inputPort) > 0) {
        if (_policies.find(inputPort)->second.isEager()) eagerInputs++;
        this->addInput(inputPort, 0, _policies.find(inputPort)->second);
      } else
        this->addInput(inputPort, 0, {});
    }
    this->addOutput("o1");
    if (eagerInputs == coeff.size())
      throw std::runtime_error(typeName() + ": at least one input port should be not eager.");
  }

  string typeName() const override { return "Linear"; }

  /*
    map<outputPort, vector<Message<T, V>>>
  */
  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> toEmit;
    int i = 0;
    Message<T, V> out;
    out.value = 0;
    for (auto it = this->inputs.begin(); it != this->inputs.end(); ++it) {
      out.value = out.value + it->second.front().value * coeff.at(i);
      if (!it->second.isEager()) out.time = it->second.front().time;
      i++;
    }
    vector<Message<T, V>> v;
    v.push_back(out);
    toEmit.emplace("o1", v);
    return toEmit;
  }
};

}  // namespace rtbot

#endif  // LINEAR_H