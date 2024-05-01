#ifndef BINARYJOIN_H
#define BINARYJOIN_H

#include <functional>
#include <optional>

#include "rtbot/Join.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct BinaryJoin : public Join<T, V> {
  BinaryJoin() = default;
  BinaryJoin(string const& id, function<optional<V>(V, V)> operation) : Join<T, V>(id) {
    this->operation = operation;
    int numPorts = 2;
    for (int i = 1; i <= numPorts; i++) {
      string inputPort = string("i") + to_string(i);
      this->addDataInput(inputPort, 0);
    }
    this->addOutput("o1");
  }

  map<string, vector<Message<T, V>>> processData() override {
    map<string, vector<Message<T, V>>> outputMsgs;
    Message<T, V> m1 = this->getDataInputMessage("i2", 0);
    Message<T, V> m0 = this->getDataInputMessage("i1", 0);
    Message<T, V> out;
    out.time = m0.time;
    optional<V> result = operation(m0.value, m1.value);
    if (!result.has_value()) return {};
    out.value = result.value();

    vector<Message<T, V>> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);

    return outputMsgs;
  }

 private:
  function<optional<V>(V, V)> operation;
};
}  // namespace rtbot

#endif  // BINARYJOIN_H
