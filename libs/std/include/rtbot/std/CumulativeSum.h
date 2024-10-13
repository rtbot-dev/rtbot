#ifndef CUMULATIVESUM_H
#define CUMULATIVESUM_H

#include <cstdint>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct CumulativeSum : public Operator<T, V> {
  CumulativeSum() = default;

  CumulativeSum(string const &id) : Operator<T, V>(id) {
    this->accumulated = 0;
    this->addDataInput("i1", CumulativeSum::size);
    this->addOutput("o1");
  }

  virtual Bytes collect() {
    Bytes bytes;

    // Serialize accumulated
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char *>(&accumulated),
                 reinterpret_cast<const unsigned char *>(&accumulated) + sizeof(accumulated));

    return bytes;
  }

  virtual void restore(Bytes::const_iterator &it) {
    // Deserialize accumulated
    accumulated = *reinterpret_cast<const V *>(&(*it));
    it += sizeof(accumulated);
  }

  string typeName() const override { return "CumulativeSum"; }

  OperatorMessage<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    OperatorMessage<T, V> outputMsgs;
    Message<T, V> out = this->getDataInputLastMessage(inputPort);
    this->accumulated = this->accumulated + out.value;
    out.value = this->accumulated;
    PortMessage<T, V> v;
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
