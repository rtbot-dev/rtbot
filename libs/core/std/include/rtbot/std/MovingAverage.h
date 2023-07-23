#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Computes the moving average.
 *   $$y(t_n)= \frac{1}{N}(x(t_n) + x(t_{n-1}) + ... + x(t_{n-N-1}))$$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   n:
 *     type: integer
 *     description: The window size, in grid steps, to be used in the computation.
 * required: ["id", "n"]
 */
template <class T, class V>
struct MovingAverage : public Operator<T, V> {
  MovingAverage() = default;

  MovingAverage(string const& id, size_t n) : Operator<T, V>(id) {
    this->addDataInput("i1", n);
    this->addOutput("o1");
  }

  string typeName() const override { return "MovingAverage"; }

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    vector<Message<T, V>> toEmit;
    Message<T, V> out;

    out.time = this->getDataInputLastMessage(inputPort).time;
    out.value = this->getDataInputSum(inputPort) / this->getDataInputSize(inputPort);

    toEmit.push_back(out);
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }
};

}  // namespace rtbot

#endif  // MOVINGAVERAGE_H
