#ifndef CONSTANTRESAMPLER_H
#define CONSTANTRESAMPLER_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct ConstantResampler : public Operator<T, V> {
  T dt;            // Time interval between emissions
  T nextEmit;      // Next time point to emit at
  bool initiated;  // Whether we've received our first message
  V lastValue;     // Last value received, used for causal consistency

  ConstantResampler() = default;

  ConstantResampler(string const& id, T interval) : Operator<T, V>(id) {
    if (interval <= 0) throw runtime_error(typeName() + ": time interval must be positive");

    this->dt = interval;
    this->initiated = false;
    this->addDataInput("i1", 1);
    this->addOutput("o1");
  }

  virtual Bytes collect() {
    Bytes bytes = Operator<T, V>::collect();

    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&dt),
                 reinterpret_cast<const unsigned char*>(&dt) + sizeof(dt));
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&nextEmit),
                 reinterpret_cast<const unsigned char*>(&nextEmit) + sizeof(nextEmit));
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&initiated),
                 reinterpret_cast<const unsigned char*>(&initiated) + sizeof(initiated));
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&lastValue),
                 reinterpret_cast<const unsigned char*>(&lastValue) + sizeof(lastValue));

    return bytes;
  }

  virtual void restore(Bytes::const_iterator& it) {
    Operator<T, V>::restore(it);

    dt = *reinterpret_cast<const T*>(&(*it));
    it += sizeof(dt);
    nextEmit = *reinterpret_cast<const T*>(&(*it));
    it += sizeof(nextEmit);
    initiated = *reinterpret_cast<const bool*>(&(*it));
    it += sizeof(initiated);
    lastValue = *reinterpret_cast<const V*>(&(*it));
    it += sizeof(lastValue);
  }

  string typeName() const override { return "ConstantResampler"; }

  OperatorMessage<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");

    Message<T, V> msg = this->getDataInputLastMessage(inputPort);

    // Initialize on first message
    if (!initiated) {
      nextEmit = msg.time + dt;
      lastValue = msg.value;
      initiated = true;
      return {};
    }

    OperatorMessage<T, V> outputMsgs;
    PortMessage<T, V> toEmit;

    // If message time is past next emit time, we need to emit at all grid points
    // up to (but not including) the current message time
    while (nextEmit < msg.time) {
      Message<T, V> out;
      out.time = nextEmit;
      out.value = lastValue;  // Use previous value for causal consistency
      toEmit.push_back(out);
      nextEmit += dt;
    }

    // If current message is exactly on the grid, emit with current value
    if (nextEmit == msg.time) {
      Message<T, V> out;
      out.time = msg.time;
      out.value = msg.value;  // Use current value for grid-aligned messages
      toEmit.push_back(out);
      nextEmit += dt;
    }

    // If we emitted anything, add it to output messages
    if (!toEmit.empty()) {
      outputMsgs.emplace("o1", toEmit);
    }

    // Update last value for future emissions
    lastValue = msg.value;

    return outputMsgs;
  }

  T getInterval() const { return dt; }
  T getNextEmissionTime() const { return nextEmit; }
  bool isInitiated() const { return initiated; }
  V getLastValue() const { return lastValue; }

 private:
  void reset() { initiated = false; }
};

}  // namespace rtbot

#endif  // CONSTANTRESAMPLER_H