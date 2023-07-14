#ifndef LINEAR_H
#define LINEAR_H

#include "rtbot/Join.h"

namespace rtbot {

using namespace std;

/**
 * @brief The Linear class as example of application of Join
 */
template <class T, class V>
struct Linear : public Join<T, V> {
  Linear() = default;
  Linear(string const& id, vector<V> const& coeff, map<string, typename Operator<T, V>::InputPolicy> policies = {})
      : Join<T, V>(id) {
    if (coeff.size() < 2) throw runtime_error(typeName() + ": number of ports have to be greater than or equal 2");
    this->coeff = coeff;
    this->notEagerPort = "";
    this->eagerPort = "";
    for (size_t i = 1; i <= this->coeff.size(); i++) {
      string inputPort = string("i") + to_string(i);
      if (policies.count(inputPort) > 0) {
        if (policies.find(inputPort)->second.isEager())
          this->eagerPort = inputPort;
        else
          this->notEagerPort = inputPort;
        this->addDataInput(inputPort, 0, policies.find(inputPort)->second);
      } else {
        this->addDataInput(inputPort, 0, {});
        this->notEagerPort = inputPort;
      }
    }
    this->addOutput("o1");
    if (this->notEagerPort.empty()) throw runtime_error(typeName() + ": at least one input port should be not eager.");
  }

  string typeName() const override { return "Linear"; }

  /*
    map<outputPort, vector<Message<T, V>>>
  */
  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    int i = 0;
    Message<T, V> out;
    out.value = 0;
    out.time = 0;
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      out.value = out.value + it->second.front().value * this->coeff.at(i);
      i++;
    }
    out.time = this->dataInputs.find(this->notEagerPort)->second.front().time;

    if (this->outputMsgs.count("o1") == 0) {
      vector<Message<T, V>> v;
      v.push_back(out);
      this->outputMsgs.emplace("o1", v);
    } else
      this->outputMsgs["o1"].push_back(out);

    return this->outputMsgs;
  }

  vector<V> getCoefficients() const { return this->coeff; }

 private:
  vector<V> coeff;
};

}  // namespace rtbot

#endif  // LINEAR_H