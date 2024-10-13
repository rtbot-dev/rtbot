
#include "rtbot/FactoryOp.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/Demultiplexer.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Operator.h"
#include "rtbot/Output.h"
#include "rtbot/Pipeline.h"
#include "rtbot/finance/RelativeStrengthIndex.h"
#include "rtbot/std/Add.h"
#include "rtbot/std/And.h"
#include "rtbot/std/AutoRegressive.h"
#include "rtbot/std/Constant.h"
#include "rtbot/std/CosineResampler.h"
#include "rtbot/std/Count.h"
#include "rtbot/std/CumulativeSum.h"
#include "rtbot/std/Difference.h"
#include "rtbot/std/Division.h"
#include "rtbot/std/EqualTo.h"
#include "rtbot/std/FiniteImpulseResponse.h"
#include "rtbot/std/GreaterThan.h"
#include "rtbot/std/GreaterThanStream.h"
#include "rtbot/std/HermiteResampler.h"
#include "rtbot/std/Identity.h"
#include "rtbot/std/LessThan.h"
#include "rtbot/std/LessThanStream.h"
#include "rtbot/std/Linear.h"
#include "rtbot/std/Minus.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/Multiplication.h"
#include "rtbot/std/Or.h"
#include "rtbot/std/PeakDetector.h"
#include "rtbot/std/Plus.h"
#include "rtbot/std/Power.h"
#include "rtbot/std/Scale.h"
#include "rtbot/std/StandardDeviation.h"
#include "rtbot/std/TimeShift.h"
#include "rtbot/std/Variable.h"

using json = nlohmann::json;

namespace rtbot {

using namespace std;

/* Operators serialization - deserialization - begin */

template <class T, class V>
void to_json(json& j, const Input<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"numPorts", p.getNumPorts()}};
}

template <class T, class V>
void from_json(const json& j, Input<T, V>& p) {
  p = Input<T, V>(j["id"].get<string>(), j.value("numPorts", 1));
}

template <class T, class V>
void to_json(json& j, const CosineResampler<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"dt", p.dt}};
}

template <class T, class V>
void from_json(const json& j, CosineResampler<T, V>& p) {
  p = CosineResampler<T, V>(j["id"].get<string>(), j["dt"].get<T>());
}

template <class T, class V>
void to_json(json& j, const HermiteResampler<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"dt", p.dt}};
}

template <class T, class V>
void from_json(const json& j, HermiteResampler<T, V>& p) {
  p = HermiteResampler<T, V>(j["id"].get<string>(), j["dt"].get<T>());
}

template <class T, class V>
void to_json(json& j, const StandardDeviation<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.getDataInputMaxSize()}};
}

template <class T, class V>
void from_json(const json& j, StandardDeviation<T, V>& p) {
  p = StandardDeviation<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const MovingAverage<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.getDataInputMaxSize()}};
}

template <class T, class V>
void from_json(const json& j, MovingAverage<T, V>& p) {
  p = MovingAverage<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const FiniteImpulseResponse<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"coeff", p.getCoefficients()}};
}

template <class T, class V>
void from_json(const json& j, FiniteImpulseResponse<T, V>& p) {
  p = FiniteImpulseResponse<T, V>(j["id"].get<string>(), j["coeff"].get<vector<V>>());
}

template <class T, class V>
void to_json(json& j, const Join<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"numPorts", p.getNumDataInputs()}};
}

template <class T, class V>
void from_json(const json& j, Join<T, V>& p) {
  p = Join<T, V>(j["id"].get<string>(), j["numPorts"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const Output<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"numPorts", p.getNumPorts()}};
}

template <class T, class V>
void from_json(const json& j, Output<T, V>& p) {
  p = Output<T, V>(j["id"].get<string>(), j.value("numPorts", 1));
}

template <class T, class V>
void to_json(json& j, const PeakDetector<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.getDataInputMaxSize()}};
}

template <class T, class V>
void from_json(const json& j, PeakDetector<T, V>& p) {
  p = PeakDetector<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const Minus<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Minus<T, V>& p) {
  p = Minus<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const And<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, And<T, V>& p) {
  p = And<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const Or<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Or<T, V>& p) {
  p = Or<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const GreaterThanStream<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, GreaterThanStream<T, V>& p) {
  p = GreaterThanStream<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const LessThanStream<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, LessThanStream<T, V>& p) {
  p = LessThanStream<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const Division<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Division<T, V>& p) {
  p = Division<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const Multiplication<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Multiplication<T, V>& p) {
  p = Multiplication<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const Plus<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Plus<T, V>& p) {
  p = Plus<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const Linear<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"coeff", p.getCoefficients()}};
}

template <class T, class V>
void from_json(const json& j, Linear<T, V>& p) {
  p = Linear<T, V>(j["id"].get<string>(), j["coeff"].get<vector<V>>());
}

template <class T, class V>
void to_json(json& j, const AutoRegressive<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"coeff", p.coeff}};
}

template <class T, class V>
void from_json(const json& j, AutoRegressive<T, V>& p) {
  p = AutoRegressive<T, V>(j["id"].get<string>(), j["coeff"].get<vector<V>>());
}

template <class T, class V>
void to_json(json& j, const GreaterThan<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"value", p.getValue()}};
}

template <class T, class V>
void from_json(const json& j, GreaterThan<T, V>& p) {
  p = GreaterThan<T, V>(j["id"].get<string>(), j["value"].get<V>());
}

template <class T, class V>
void to_json(json& j, const Constant<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"value", p.getValue()}};
}

template <class T, class V>
void from_json(const json& j, Constant<T, V>& p) {
  p = Constant<T, V>(j["id"].get<string>(), j["value"].get<V>());
}

template <class T, class V>
void to_json(json& j, const Add<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"value", p.getValue()}};
}

template <class T, class V>
void from_json(const json& j, Add<T, V>& p) {
  p = Add<T, V>(j["id"].get<string>(), j["value"].get<V>());
}

template <class T, class V>
void to_json(json& j, const LessThan<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"value", p.getValue()}};
}

template <class T, class V>
void from_json(const json& j, LessThan<T, V>& p) {
  p = LessThan<T, V>(j["id"].get<string>(), j["value"].get<V>());
}

template <class T, class V>
void to_json(json& j, const EqualTo<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"value", p.getValue()}};
}

template <class T, class V>
void from_json(const json& j, EqualTo<T, V>& p) {
  p = EqualTo<T, V>(j["id"].get<string>(), j["value"].get<V>());
}

template <class T, class V>
void to_json(json& j, const Variable<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"value", p.getValue()}};
}

template <class T, class V>
void from_json(const json& j, Variable<T, V>& p) {
  p = Variable<T, V>(j["id"].get<string>(), j.value("value", 0));
}

template <class T, class V>
void to_json(json& j, const TimeShift<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"dt", p.getDT()}, {"times", p.getTimes()}};
}

template <class T, class V>
void from_json(const json& j, TimeShift<T, V>& p) {
  p = TimeShift<T, V>(j["id"].get<string>(), j.value("dt", 1), j.value("times", 1));
}

template <class T, class V>
void to_json(json& j, const CumulativeSum<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, CumulativeSum<T, V>& p) {
  p = CumulativeSum<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const Count<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Count<T, V>& p) {
  p = Count<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const Scale<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"value", p.getValue()}};
}

template <class T, class V>
void from_json(const json& j, Scale<T, V>& p) {
  p = Scale<T, V>(j["id"].get<string>(), j["value"].get<V>());
}

template <class T, class V>
void to_json(json& j, const Power<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"value", p.getValue()}};
}

template <class T, class V>
void from_json(const json& j, Power<T, V>& p) {
  p = Power<T, V>(j["id"].get<string>(), j["value"].get<V>());
}

template <class T, class V>
void to_json(json& j, const Difference<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Difference<T, V>& p) {
  p = Difference<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const Demultiplexer<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"numPorts", p.getNumOutputPorts()}};
}

template <class T, class V>
void from_json(const json& j, Demultiplexer<T, V>& p) {
  p = Demultiplexer<T, V>(j["id"].get<string>(), j.value("numPorts", 1));
}

template <class T, class V>
void to_json(json& j, const Identity<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Identity<T, V>& p) {
  p = Identity<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const RelativeStrengthIndex<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.getDataInputMaxSize()}};
}

template <class T, class V>
void from_json(const json& j, RelativeStrengthIndex<T, V>& p) {
  p = RelativeStrengthIndex<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const Pipeline<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"prog", json::parse(p.json_prog)}};
}

template <class T, class V>
void from_json(const json& j, Pipeline<T, V>& p) {
  p = Pipeline<T, V>(j.at("id").get<string>(), j.at("prog").dump());
}

/* Operators serialization - deserialization - end */

Bytes FactoryOp::serialize(string const& programId) {
  if (this->programs.count(programId) == 0) throw runtime_error("Program " + programId + " was not found");
  return programs.at(programId).serialize();
}

string FactoryOp::createProgram(string const& id, Bytes const& bytes) {
  if (this->programs.count(id) > 0) throw runtime_error("Program " + id + " already exists");
  programs.emplace(id, Program(bytes));
  return id;
}

string FactoryOp::createProgram(string const& id, string const& json_program) {
  try {
    programs.emplace(id, Program(json_program));
    return "";
  } catch (const json::parse_error& e) {
    // output exception information
    cout << "message: " << e.what() << '\n'
         << "exception id: " << e.id << '\n'
         << "byte position of error: " << e.byte << endl;
    return string("Unable to parse program: ") + e.what();
  }
}

/// register some the operators. Notice that this can be done on any constructor
/// whenever we create a static instance later, as  below:
FactoryOp::FactoryOp() {
  op_registry_add<Input<uint64_t, double>, json>();
  op_registry_add<CosineResampler<uint64_t, double>, json>();
  op_registry_add<HermiteResampler<uint64_t, double>, json>();
  op_registry_add<MovingAverage<uint64_t, double>, json>();
  op_registry_add<FiniteImpulseResponse<uint64_t, double>, json>();
  op_registry_add<StandardDeviation<uint64_t, double>, json>();
  op_registry_add<PeakDetector<uint64_t, double>, json>();
  op_registry_add<Join<uint64_t, double>, json>();
  op_registry_add<Minus<uint64_t, double>, json>();
  op_registry_add<And<uint64_t, double>, json>();
  op_registry_add<Or<uint64_t, double>, json>();
  op_registry_add<Division<uint64_t, double>, json>();
  op_registry_add<Multiplication<uint64_t, double>, json>();
  op_registry_add<Plus<uint64_t, double>, json>();
  op_registry_add<Linear<uint64_t, double>, json>();
  op_registry_add<AutoRegressive<uint64_t, double>, json>();
  op_registry_add<Output<uint64_t, double>, json>();
  op_registry_add<GreaterThan<uint64_t, double>, json>();
  op_registry_add<LessThan<uint64_t, double>, json>();
  op_registry_add<EqualTo<uint64_t, double>, json>();
  op_registry_add<Scale<uint64_t, double>, json>();
  op_registry_add<Constant<uint64_t, double>, json>();
  op_registry_add<CumulativeSum<uint64_t, double>, json>();
  op_registry_add<Count<uint64_t, double>, json>();
  op_registry_add<Add<uint64_t, double>, json>();
  op_registry_add<Difference<uint64_t, double>, json>();
  op_registry_add<Demultiplexer<uint64_t, double>, json>();
  op_registry_add<Power<uint64_t, double>, json>();
  op_registry_add<Identity<uint64_t, double>, json>();
  op_registry_add<RelativeStrengthIndex<uint64_t, double>, json>();
  op_registry_add<Variable<uint64_t, double>, json>();
  op_registry_add<TimeShift<uint64_t, double>, json>();
  op_registry_add<Pipeline<uint64_t, double>, json>();
  op_registry_add<GreaterThanStream<uint64_t, double>, json>();
  op_registry_add<LessThanStream<uint64_t, double>, json>();
}

static FactoryOp factory;

Op_ptr<uint64_t, double> FactoryOp::readOp(const string& program) {
  auto json = json::parse(program);
  string type = json.at("type");
  auto it = op_registry().find(type);
  if (it == op_registry().end()) throw runtime_error(string("invalid Operator type while parsing ") + type);
  return it->second.from_string(program);
}

string FactoryOp::writeOp(Op_ptr<uint64_t, double> const& op) {
  string type = op->typeName();
  auto it = op_registry().find(type);
  if (it == op_registry().end()) throw runtime_error(string("invalid Operator type while parsing ") + type);
  return it->second.to_string(op);
}

}  // namespace rtbot
