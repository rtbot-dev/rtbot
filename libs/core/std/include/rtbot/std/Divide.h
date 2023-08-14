#ifndef DIVIDE_H
#define DIVIDE_H

#include "rtbot/Join.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Synchronizes two streams and computes its division. Synchronization mechanism inherited from `Join`.
 *   $$y(t_n)=x_1(t_n) / x_2(t_n)$$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 * required: ["id"]
 */
template <class T, class V>
struct Divide : public Join<T, V> {
  Divide() = default;
  Divide(string const &id) : Join<T, V>(id) {
    int numPorts = 2;

    for (int i = 1; i <= numPorts; i++) {
      string inputPort = string("i") + to_string(i);
      this->addDataInput(inputPort, 0);
    }
    this->addOutput("o1");
  }

  string typeName() const override { return "Divide"; }

  map<string, vector<Message<T, V>>> processData() override {
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> m1 = this->getDataInputFirstMessage("i2");
    Message<T, V> m0 = this->getDataInputFirstMessage("i1");
    Message<T, V> out;
    out.time = m0.time;
    out.value = m0.value / m1.value;

    vector<Message<T, V>> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);

    return outputMsgs;
  }
};

}  // namespace rtbot

#endif  // DIVIDE_H