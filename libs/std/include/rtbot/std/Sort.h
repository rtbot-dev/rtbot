#include <algorithm>

#include "rtbot/Join.h"

namespace rtbot {

template <class T, class V>
class Sort : public Join<T, V> {
 private:
  size_t numOutputs;
  size_t numInputs;
  bool ascending;

 public:
  Sort(string const& id, size_t numInputs, size_t numOutputs, bool ascending = true, size_t maxInputBufferSize = 100)
      : Join<T, V>(id) {
    if (numOutputs > numInputs) throw std::runtime_error("Sort: numOutputs must be less than or equal to numInputs");
    this->numOutputs = numOutputs;
    this->numInputs = numInputs;
    this->ascending = ascending;

    // Register the inputs
    for (size_t i = 0; i < numInputs; i++) {
      this->addDataInput("i" + std::to_string(i + 1), maxInputBufferSize);
    }

    // Register the outputs
    for (size_t i = 0; i < numOutputs; i++) {
      this->addOutput("o" + std::to_string(i + 1));
    }
  }

  string typeName() const override { return "Sort"; }

  map<string, vector<Message<T, V>>> processData() override {
    // Get the input data
    auto inputs = this->getDataInputs();

    vector<size_t> idx(this->numInputs);
    iota(idx.begin(), idx.end(), 0);

    // see https://stackoverflow.com/questions/1577475/c-sorting-and-keeping-track-of-indexes
    stable_sort(idx.begin(), idx.end(), [this](size_t i1, size_t i2) {
      auto v1 = this->dataInputs.find("i" + std::to_string(i1 + 1))->second.front().value;
      auto v2 = this->dataInputs.find("i" + std::to_string(i2 + 1))->second.front().value;
      return this->ascending ? v1 < v2 : v1 > v2;
    });

    map<string, vector<Message<T, V>>> outputMsgs;
    // take only the first numOutputs elements
    for (size_t i = 0; i < this->numOutputs; i++) {
      auto inputPort = "i" + std::to_string(idx[i] + 1);
      auto outputPort = "o" + std::to_string(i + 1);
      Message<T, V> out = this->dataInputs.find(inputPort)->second.front();
      vector<Message<T, V>> v;
      v.push_back(out);
      outputMsgs.emplace(outputPort, v);
    }

    return outputMsgs;
  }
};

}  // namespace rtbot