#ifndef TIMESORT_H
#define TIMESORT_H

#include "rtbot/Operator.h"

namespace rtbot {

using namespace std;

template <class T, class V>
class TimeSort : public Operator<T, V> {
 public:
  TimeSort() = default;
  TimeSort(string const &id, size_t numPorts, bool increasing = true) : Operator<T, V>(id) {
    this->increasing = increasing;
    for (int i = 1; i <= numPorts; i++) {
      string inputPort = string("i") + to_string(i);
      string outputPort = string("o") + to_string(i);
      this->addDataInput(inputPort);
      this->addOutput(outputPort);
    }
  }
  virtual ~TimeSort() = default;

  virtual string typeName() const override { return "TimeSort"; }

  map<string, map<string, vector<Message<T, V>>>> receiveData(Message<T, V> msg, string inputPort = "") override {
    if (inputPort.empty()) {
      auto in = this->getDataInputs();
      if (in.size() == 1) inputPort = in.at(0);
    }

    if (this->dataInputs.count(inputPort) > 0) {
      this->dataInputs.find(inputPort)->second.push_back(msg);
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() +
                                                      this->dataInputs.find(inputPort)->second.back().value);
    } else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");

    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      if (it->second.empty()) return {};
    }

    auto toEmit = processData(inputPort);
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      it->second.setSum(it->second.getSum() - it->second.front().value);
      it->second.pop_front();
    }
    return this->emit(toEmit);
  }

  /*
    map<outputPort, vector<Message<T, V>>>
  */
  virtual map<string, vector<Message<T, V>>> processData(string inputPort) {
    map<string, vector<Message<T, V>>> outputMsgs;

    vector<Message<T, V>> v;
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      v.push_back(it->second.front());
    }

    if (this->increasing)
      sort(v.begin(), v.end(), [](const Message<T, V> &a, const Message<T, V> &b) { return a.time < b.time; });
    else
      sort(v.begin(), v.end(), [](const Message<T, V> &a, const Message<T, V> &b) { return a.time > b.time; });

    for (int i = 0; i < v.size(); i++) {
      vector<Message<T, V>> x;
      x.push_back(v.at(i));
      outputMsgs.emplace(string("o") + to_string(i + 1), x);
    }

    return outputMsgs;
  }

 private:
  bool increasing;
};

}  // end namespace rtbot

#endif  // TIMESORT_H
