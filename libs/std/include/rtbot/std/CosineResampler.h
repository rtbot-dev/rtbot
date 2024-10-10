#ifndef COSINERESAMPLER_H
#define COSINERESAMPLER_H

#include <cmath>
#include <cstdint>
#include <vector>

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct CosineResampler : public Operator<T, V> {
  static const size_t size = 2;
  T dt;
  T carryOver;

  CosineResampler() = default;

  CosineResampler(string const &id, T dt) : Operator<T, V>(id) {
    this->dt = dt;
    this->carryOver = 0;
    this->addDataInput("i1", CosineResampler::size);
    this->addOutput("o1");
  }

  virtual Bytes collect() {
    Bytes bytes = Operator<T, V>::collect();

    // Serialize carryOver
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char *>(&carryOver),
                 reinterpret_cast<const unsigned char *>(&carryOver) + sizeof(carryOver));

    return bytes;
  }

  virtual void restore(Bytes::const_iterator &it) {
    Operator<T, V>::restore(it);
    // Deserialize carryOver
    carryOver = *reinterpret_cast<const T *>(&(*it));
    it += sizeof(carryOver);
  }

  string typeName() const override { return "CosineResampler"; }

  OperatorMessage<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");
    OperatorMessage<T, V> outputMsgs;
    PortMessage<T, V> toEmit;

    int j = 1;

    while (this->getDataInputMessage(inputPort, 1).time - this->getDataInputMessage(inputPort, 0).time >=
           (j * dt) - carryOver) {
      Message<T, V> out;
      V mu = (V)((j * dt) - carryOver) /
             (V)(this->getDataInputMessage(inputPort, 1).time - this->getDataInputMessage(inputPort, 0).time);
      out.value = CosineResampler<T, V>::cosineInterpolate(this->getDataInputMessage(inputPort, 0).value,
                                                           this->getDataInputMessage(inputPort, 1).value, mu);
      out.time = this->getDataInputMessage(inputPort, 0).time + ((j * dt) - carryOver);
      toEmit.push_back(out);
      j++;
    }

    carryOver = this->getDataInputMessage(inputPort, 1).time -
                (this->getDataInputMessage(inputPort, 0).time + (((j - 1) * dt) - carryOver));

    if (toEmit.size() > 0) {
      outputMsgs.emplace("o1", toEmit);
      return outputMsgs;
    }
    return {};
  }

 private:
  /**
   * Calculations taken from http://paulbourke.net/miscellaneous/interpolation/
   */
  static V cosineInterpolate(V y1, V y2, V mu) {
    V mu2;
    mu2 = (1 - cos(mu * 3.1415926535897932)) / 2;
    return (y1 * (1 - mu2) + y2 * mu2);
  }
};

}  // namespace rtbot

#endif  // COSINERESAMPLER_H
