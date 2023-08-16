#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <algorithm>
#include <memory>

#include "rtbot/Demultiplexer.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Operator.h"
#include "rtbot/Output.h"
#include "rtbot/finance/RelativeStrengthIndex.h"
#include "rtbot/std/Add.h"
#include "rtbot/std/Autoregressive.h"
#include "rtbot/std/Constant.h"
#include "rtbot/std/CosineResampler.h"
#include "rtbot/std/Count.h"
#include "rtbot/std/CumulativeSum.h"
#include "rtbot/std/Difference.h"
#include "rtbot/std/Division.h"
#include "rtbot/std/EqualTo.h"
#include "rtbot/std/GreaterThan.h"
#include "rtbot/std/HermiteResampler.h"
#include "rtbot/std/Identity.h"
#include "rtbot/std/LessThan.h"
#include "rtbot/std/Linear.h"
#include "rtbot/std/Minus.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "rtbot/std/Power.h"
#include "rtbot/std/Scale.h"
#include "rtbot/std/StandardDeviation.h"
#include "rtbot/std/TimeShift.h"
#include "rtbot/std/Variable.h"

namespace rtbot {

using namespace std;

template <class T, class V>
struct Composite : public Operator<T, V>  // TODO: improve from chain to graph
{
  Composite() = default;

  Composite(string const &id) : Operator<T, V>(id) { this->input == nullptr; }

  string typeName() const override { return "Composite"; }

  virtual ~Composite() = default;

  string createInput(string id, size_t numPorts = 1) {
    if (this->ops.count(id) == 0 && this->input == nullptr) {
      this->input = make_shared<Input<T, V>>(id, numPorts);
      vector<string> dataI = this->input->getDataInputs();
      vector<string> controlI = this->input->getControlInputs();
      for (int i = 0; i < dataI.size(); i++) this->addDataInput(dataI.at(i), 1);
      for (int i = 0; i < controlI.size(); i++) this->addControlInput(controlI.at(i), 1);
      this->ops.emplace(id, this->input);
    } else if (this->ops.count(id) > 0)
      throw std::runtime_error(typeName() + ": unique id is required");
    else if (this->input != nullptr)
      throw std::runtime_error(typeName() + ": input operator already setup");
    return id;
  }

  string createAdd(string id, V value) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Add<T, V>>(id, value));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createAutoregressive(string id, vector<V> const &coeff) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<AutoRegressive<T, V>>(id, coeff));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createConstant(string id, V value) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Constant<T, V>>(id, value));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createCosineResampler(string id, T dt) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<CosineResampler<T, V>>(id, dt));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createCount(string id) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Count<T, V>>(id));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createCumulativeSum(string id) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<CumulativeSum<T, V>>(id));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createDifference(string id, bool useOldestTime = true) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Difference<T, V>>(id, useOldestTime));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createJoin(string id, size_t numPorts) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Join<T, V>>(id, numPorts));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createDivision(string id) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Division<T, V>>(id));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createMinus(string id) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Minus<T, V>>(id));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createLinear(string id, vector<V> const &coeff) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Linear<T, V>>(id, coeff));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createEqualTo(string id, V value) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<EqualTo<T, V>>(id, value));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createGreaterThan(string id, V value) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<GreaterThan<T, V>>(id, value));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createLessThan(string id, V value) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<LessThan<T, V>>(id, value));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createHermiteResampler(string id, T dt) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<HermiteResampler<T, V>>(id, dt));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createIdentity(string id) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Identity<T, V>>(id));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createMovingAverage(string id, size_t n) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<MovingAverage<T, V>>(id, n));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createVariable(string id, V value = 0) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Variable<T, V>>(id, value));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createOutput(string id, size_t numPorts = 1) {
    if (this->ops.count(id) == 0 && this->output == nullptr) {
      this->output = make_shared<Output_vec<T, V>>(id, numPorts);
      vector<string> outs = this->output->getOutputs();
      for (int i = 0; i < outs.size(); i++) this->addOutput(outs.at(i));
      this->ops.emplace(id, this->output);
    } else if (this->ops.count(id) > 0)
      throw std::runtime_error(typeName() + ": unique id is required");
    else if (this->output != nullptr)
      throw std::runtime_error(typeName() + ": output operator already setup");

    return id;
  }

  string createPeakDetector(string id, size_t n) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<PeakDetector<T, V>>(id, n));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createStandardDeviation(string id, size_t n) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<StandardDeviation<T, V>>(id, n));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createDemultiplexer(string id, size_t numOutputPorts = 2) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Demultiplexer<T, V>>(id, numOutputPorts));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createPower(string id, V value) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Power<T, V>>(id, value));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createScale(string id, V value) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<Scale<T, V>>(id, value));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  string createTimeShift(string id, T dt = 1, int times = 1) {
    if (this->ops.count(id) == 0) {
      this->ops.emplace(id, make_shared<TimeShift<T, V>>(id, dt, times));
      return id;
    } else
      throw std::runtime_error(typeName() + ": unique id is required");
  }

  shared_ptr<Operator<T, V>> getOperator(string id) {
    if (this->ops.count(id) == 1) {
      return this->ops.find(id)->second;
    } else
      throw std::runtime_error(typeName() + ": operator now found");
  }

  Operator<T, V> *createInternalConnection(string operatorId, string childId, string outputPort = "",
                                           string inputPort = "") {
    if (this->ops.count(operatorId) == 0)
      throw std::runtime_error(typeName() + ": operator " + operatorId +
                               " has not been added to the operator list, please use add[Operator] first");
    if (this->ops.count(childId) == 0)
      throw std::runtime_error(typeName() + ": operator " + childId +
                               " has not been added to the operator list, please use add[Operator] first");
    auto op = this->ops.find(operatorId)->second;
    auto child = this->ops.find(childId)->second;
    auto childptr = op.get()->connect(child.get(), outputPort, inputPort);
    if (childptr != nullptr)
      return this;
    else
      throw std::runtime_error(typeName() + ": connection was not successful");
  }

  virtual void receiveData(Message<T, V> msg, string inputPort = "") override {
    if (this->input != nullptr)
      this->input.get()->receiveData(msg, inputPort);
    else
      throw std::runtime_error(typeName() + ": the input operator have not been found");
  }

  virtual map<string, map<string, vector<Message<T, V>>>> executeData() override {
    if (this->input != nullptr)
      return this->input.get()->executeData();
    else
      throw std::runtime_error(typeName() + ": the input operator have not been found");
  }

  virtual void receiveControl(Message<T, V> msg, string inputPort = "") override {
    if (this->input != nullptr)
      this->input.get()->receiveControl(msg, inputPort);
    else
      throw std::runtime_error(typeName() + ": the input operator have not been found");
  }

  virtual map<string, map<string, vector<Message<T, V>>>> executeControl() override {
    if (this->input != nullptr)
      return this->input.get()->executeControl();
    else
      throw std::runtime_error(typeName() + ": the input operator have not been found");
  }

  virtual map<string, vector<Message<T, V>>> processData() override { return {}; }

  virtual map<string, vector<Message<T, V>>> processControl() override { return {}; }

  virtual Operator<T, V> *connect(Operator<T, V> &child, string outputPort = "", string inputPort = "") override {
    if (this->output != nullptr)
      return this->output->connect(child, outputPort, inputPort);
    else if (this->output == nullptr)
      throw std::runtime_error(typeName() + ": output operator have not been found");
    return nullptr;
  }

  virtual Operator<T, V> *connect(Operator<T, V> *child, string outputPort = "", string inputPort = "") override {
    if (this->output != nullptr)
      return this->output->connect(child, outputPort, inputPort);
    else if (this->output == nullptr)
      throw std::runtime_error(typeName() + ": output operator have not been found");
    return nullptr;
  }

 private:
  map<string, shared_ptr<Operator<T, V>>> ops;
  shared_ptr<Operator<T, V>> input;
  shared_ptr<Operator<T, V>> output;
};

}  // namespace rtbot

#endif  // COMPOSITE_H
