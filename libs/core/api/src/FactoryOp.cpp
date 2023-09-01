
#include "rtbot/FactoryOp.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/Demultiplexer.h"
#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Operator.h"
#include "rtbot/Output.h"
#include "rtbot/finance/RelativeStrengthIndex.h"
#include "rtbot/std/Add.h"
#include "rtbot/std/AutoRegressive.h"
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
#include "rtbot/std/Multiplication.h"
#include "rtbot/std/PeakDetector.h"
#include "rtbot/std/Plus.h"
#include "rtbot/std/Power.h"
#include "rtbot/std/Scale.h"
#include "rtbot/std/StandardDeviation.h"
#include "rtbot/std/TimeShift.h"
#include "rtbot/std/Variable.h"

using json = nlohmann::json;

namespace rtbot {

/* Operators serialization - deserialization - begin */

/*
{
    "type": "Input",
    "id": "in",
    "numPorts": 1
}
*/

/*
{
    "type": "Input",
    "id": "in"
}
*/
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
void to_json(json& j, const Join<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"numPorts", p.getNumDataInputs()}};
}

template <class T, class V>
void from_json(const json& j, Join<T, V>& p) {
  p = Join<T, V>(j["id"].get<string>(), j["numPorts"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const Output_opt<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"numPorts", p.getNumPorts()}};
}

template <class T, class V>
void from_json(const json& j, Output_opt<T, V>& p) {
  p = Output_opt<T, V>(j["id"].get<string>(), j.value("numPorts", 1));
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
  j = json{{"type", p.typeName()}, {"id", p.id}, {"useOldestTime", p.getUseOldestTime()}};
}

template <class T, class V>
void from_json(const json& j, Difference<T, V>& p) {
  p = Difference<T, V>(j["id"].get<string>(), j.value("useOldestTime", true));
}

template <class T, class V>
void to_json(json& j, const Demultiplexer<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"numOutputPorts", p.getNumOutputPorts()}};
}

template <class T, class V>
void from_json(const json& j, Demultiplexer<T, V>& p) {
  p = Demultiplexer<T, V>(j["id"].get<string>(), j.value("numOutputPorts", 1));
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

/* Operators serialization - deserialization - end */

std::string FactoryOp::createProgram(std::string const& id, std::string const& json_program) {
  try {
    programs.emplace(id, createProgram(json_program));
    return "";
  } catch (const json::parse_error& e) {
    // output exception information
    std::cout << "message: " << e.what() << '\n'
              << "exception id: " << e.id << '\n'
              << "byte position of error: " << e.byte << std::endl;
    return std::string("Unable to parse program: ") + e.what();
  }
}

/// register some the operators. Notice that this can be done on any constructor
/// whenever we create a static instance later, as  below:
FactoryOp::FactoryOp() {
  op_registry_add<Input<std::uint64_t, double>, json>();
  op_registry_add<CosineResampler<std::uint64_t, double>, json>();
  op_registry_add<HermiteResampler<std::uint64_t, double>, json>();
  op_registry_add<MovingAverage<std::uint64_t, double>, json>();
  op_registry_add<StandardDeviation<std::uint64_t, double>, json>();
  op_registry_add<PeakDetector<std::uint64_t, double>, json>();
  op_registry_add<Join<std::uint64_t, double>, json>();
  op_registry_add<Minus<std::uint64_t, double>, json>();
  op_registry_add<Division<std::uint64_t, double>, json>();
  op_registry_add<Multiplication<std::uint64_t, double>, json>();
  op_registry_add<Plus<std::uint64_t, double>, json>();
  op_registry_add<Linear<std::uint64_t, double>, json>();
  op_registry_add<AutoRegressive<std::uint64_t, double>, json>();
  op_registry_add<Output_opt<std::uint64_t, double>, json>();
  op_registry_add<GreaterThan<std::uint64_t, double>, json>();
  op_registry_add<LessThan<std::uint64_t, double>, json>();
  op_registry_add<EqualTo<std::uint64_t, double>, json>();
  op_registry_add<Scale<std::uint64_t, double>, json>();
  op_registry_add<Constant<std::uint64_t, double>, json>();
  op_registry_add<CumulativeSum<std::uint64_t, double>, json>();
  op_registry_add<Count<std::uint64_t, double>, json>();
  op_registry_add<Add<std::uint64_t, double>, json>();
  op_registry_add<Difference<std::uint64_t, double>, json>();
  op_registry_add<Demultiplexer<std::uint64_t, double>, json>();
  op_registry_add<Power<std::uint64_t, double>, json>();
  op_registry_add<Identity<std::uint64_t, double>, json>();
  op_registry_add<RelativeStrengthIndex<std::uint64_t, double>, json>();
  op_registry_add<Variable<std::uint64_t, double>, json>();
  op_registry_add<TimeShift<std::uint64_t, double>, json>();

  json j;
  for (auto const& it : op_registry()) {
    json ji = json::parse(it.second.to_string_default());
    j.push_back(ji);
  }
  std::ofstream out("op_list.json");
  out << std::setw(4) << j << "\n";
}

static FactoryOp factory;

Op_ptr<std::uint64_t, double> FactoryOp::readOp(const std::string& program) {
  auto json = json::parse(program);
  string type = json.at("type");
  auto it = op_registry().find(type);
  if (it == op_registry().end()) throw std::runtime_error(string("invalid Operator type while parsing ") + type);
  return it->second.from_string(program);
}

std::string FactoryOp::writeOp(Op_ptr<std::uint64_t, double> const& op) {
  string type = op->typeName();
  auto it = op_registry().find(type);
  if (it == op_registry().end()) throw std::runtime_error(string("invalid Operator type while parsing ") + type);
  return it->second.to_string(op);
}

}  // namespace rtbot
