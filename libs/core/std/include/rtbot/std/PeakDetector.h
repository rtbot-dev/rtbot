#ifndef PEAKDETECTOR_H
#define PEAKDETECTOR_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;
/**
 * @jsonschema
 * type: object
 * description: |
 *   Finds a local extreme within the time window specified.
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
struct PeakDetector : Operator<T, V> {
  PeakDetector() = default;

  PeakDetector(string const& id, size_t n) : Operator<T, V>(id) {
    this->addDataInput("i1", n);
    this->addOutput("o1");
  }

  string typeName() const override { return "PeakDetector"; }

  map<string, vector<Message<T, V>>> processData(string inputPort) override {
    map<string, vector<Message<T, V>>> outputMsgs;
    vector<Message<T, V>> toEmit;
    size_t size = this->getDataInputSize(inputPort);
    size_t pos = size / 2;  // expected position of the max
    for (auto i = 0u; i < size; i++)
      if (this->getDataInputMessage(inputPort, pos).value < this->getDataInputMessage(inputPort, i).value) return {};

    toEmit.push_back(this->getDataInputMessage(inputPort, pos));
    outputMsgs.emplace("o1", toEmit);
    return outputMsgs;
  }
};

}  // end namespace rtbot

#endif  // PEAKDETECTOR_H
