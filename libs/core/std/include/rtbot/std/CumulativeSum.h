#ifndef CUMULATIVESUM_H
#define CUMULATIVESUM_H

#include <cstdint>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Outputs the cumulative sum of the message's values that pass through it.
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 * required: ["id"]
 */
template <class T, class V>
struct CumulativeSum : public Operator<T, V> {
  CumulativeSum() = default;
  CumulativeSum(string const &id) : Operator<T, V>(id) {
    this->accumulated = 0;
    this->addDataInput("i1", CumulativeSum::size);
    this->addOutput("o1");
  }
  string typeName() const override { return "CumulativeSum"; }
  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    this->accumulated = this->accumulated + out.value;
    out.value = this->accumulated;
    vector<Message<T, V>> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }

 private:
  static const size_t size = 1;
  V accumulated;
};

}  // namespace rtbot

#endif  // CUMULATIVESUM_H
