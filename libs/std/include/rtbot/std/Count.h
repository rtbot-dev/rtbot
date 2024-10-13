#ifndef COUNT_H
#define COUNT_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Count : public Operator<T, V> {
  size_t count;

  Count() = default;

  Count(string const &id) : Operator<T, V>(id) {
    this->count = 0;
    this->addDataInput("i1", 1);
    this->addOutput("o1");
  }

  virtual Bytes collect() {
    Bytes bytes = Operator<T, V>::collect();
    // Serialize count
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char *>(&count),
                 reinterpret_cast<const unsigned char *>(&count) + sizeof(count));
    return bytes;
  }

  virtual void restore(Bytes::const_iterator &it) {
    Operator<T, V>::restore(it);
    // Deserialize count
    count = *reinterpret_cast<const size_t *>(&(*it));
    it += sizeof(count);
  }

  string typeName() const override { return "Count"; }

  string debug() {
    string s = "count: " + to_string(count);
    return Operator<T, V>::debug(s);
  }

  OperatorMessage<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    OperatorMessage<T, V> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    this->count = this->count + 1;
    out.value = this->count;
    PortMessage<T, V> v;
    v.push_back(out);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }
};

}  // namespace rtbot

#endif  // COUNT_H
