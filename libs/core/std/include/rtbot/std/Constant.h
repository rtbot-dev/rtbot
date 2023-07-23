#ifndef CONST_H
#define CONST_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   A constant operator. Always emits the same value: $y(t_n)=C$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   constant:
 *     type: number
 *     description: The constant to emit when required.
 * required: ["id", "constant"]
 */
template <class T, class V>
struct Constant : public Operator<T, V> {
  Constant() = default;
  Constant(string const &id, V constant) : Operator<T, V>(id) {
    this->addDataInput("i1", Constant::size);
    this->addOutput("o1");
    this->constant = constant;
  }
  string typeName() const override { return "Constant"; }
  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    out.value = this->constant;
    vector<Message<T, V>> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }

  V getConstant() const { return this->constant; }

 private:
  static const size_t size = 1;
  V constant;
};

}  // namespace rtbot

#endif  // CONST_H
