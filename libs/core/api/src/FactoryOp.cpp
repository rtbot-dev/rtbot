
#include "rtbot/FactoryOp.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Operator.h"
#include "rtbot/Output.h"
#include "rtbot/finance/RelativeStrengthIndex.h"
#include "rtbot/std/Accumulator.h"
#include "rtbot/std/Autoregressive.h"
#include "rtbot/std/CosineResampler.h"
#include "rtbot/std/Count.h"
#include "rtbot/std/Difference.h"
#include "rtbot/std/Divide.h"
#include "rtbot/std/GreaterThan.h"
#include "rtbot/std/HermiteResampler.h"
#include "rtbot/std/LessThan.h"
#include "rtbot/std/Linear.h"
#include "rtbot/std/Minus.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "rtbot/std/StandardDeviation.h"

using json = nlohmann::json;

namespace rtbot {

/* Operators serialization - deserialization - begin */

template <class T, class V>
void updateInputPolicyMap(const json j, map<string, typename Operator<T, V>::InputPolicy>& policies) {
  if (j.find("policies") != j.end()) {
    for (auto it = j.at("policies").begin(); it != j.at("policies").end(); ++it) {
      string port = it.key();
      bool eager = (*it).value("eager", false);
      policies.emplace(port, typename Operator<T, V>::InputPolicy(eager));
    }
  }
}

template <class T, class V>
void addPoliciesToJson(json& j, map<string, typename Operator<T, V>::InputPolicy> policies) {
  for (auto it = policies.begin(); it != policies.end(); ++it) {
    j["policies"][it->first] = json{{"eager", it->second.isEager()}};
  }
}

/*
{
    "type": "Input",
    "id": "in"
}
*/

template <class T, class V>
void to_json(json& j, const Input<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Input<T, V>& p) {
  p = Input<T, V>(j["id"].get<string>());
}

/*
{
    "type": "CosineResampler",
    "id": "cr",
    "dt": 100
}
*/

template <class T, class V>
void to_json(json& j, const CosineResampler<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"dt", p.dt}};
}

template <class T, class V>
void from_json(const json& j, CosineResampler<T, V>& p) {
  p = CosineResampler<T, V>(j["id"].get<string>(), j["dt"].get<T>());
}

/*
{
    "type": "HermiteResampler",
    "id": "hr",
    "dt": 100
}
*/

template <class T, class V>
void to_json(json& j, const HermiteResampler<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"dt", p.dt}};
}

template <class T, class V>
void from_json(const json& j, HermiteResampler<T, V>& p) {
  p = HermiteResampler<T, V>(j["id"].get<string>(), j["dt"].get<T>());
}

/*
{
    "type": "StandardDeviation",
    "id": "sd",
    "n": 5
}
*/

template <class T, class V>
void to_json(json& j, const StandardDeviation<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.getMaxSize()}};
}

template <class T, class V>
void from_json(const json& j, StandardDeviation<T, V>& p) {
  p = StandardDeviation<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

/*
{
    "type": "MovingAverage",
    "id": "ma",
    "n": 5
}
*/

template <class T, class V>
void to_json(json& j, const MovingAverage<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.getMaxSize()}};
}

template <class T, class V>
void from_json(const json& j, MovingAverage<T, V>& p) {
  p = MovingAverage<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

/*
{
    "type": "Join",
    "id": "j1",
    "numPorts": 2,
    "policies": {
      "i1": {
        "eager": true
      }
    }
}
*/

template <class T, class V>
void to_json(json& j, const Join<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"numPorts", p.getNumInputs()}};
  addPoliciesToJson<T, V>(j, p.getPolicies());
}

template <class T, class V>
void from_json(const json& j, Join<T, V>& p) {
  map<string, typename Operator<T, V>::InputPolicy> policies;
  updateInputPolicyMap<T, V>(j, policies);

  p = Join<T, V>(j["id"].get<string>(), j["numPorts"].get<size_t>(), policies);
}

/*
{
    "type": "Output",
    "id": "opt"
}
*/

template <class T, class V>
void to_json(json& j, const Output_opt<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Output_opt<T, V>& p) {
  p = Output_opt<T, V>(j["id"].get<string>());
}

/*
{
    "type": "PeakDetector",
    "id": "pd",
    "n": 5
}
*/

template <class T, class V>
void to_json(json& j, const PeakDetector<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.getMaxSize()}};
}

template <class T, class V>
void from_json(const json& j, PeakDetector<T, V>& p) {
  p = PeakDetector<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

/*
{
    "type": "Minus",
    "id": "m1",
    "policies": {
      "i1": {
        "eager": true
      }
    }
}
*/

template <class T, class V>
void to_json(json& j, const Minus<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
  addPoliciesToJson<T, V>(j, p.getPolicies());
}

template <class T, class V>
void from_json(const json& j, Minus<T, V>& p) {
  map<string, typename Operator<T, V>::InputPolicy> policies;
  updateInputPolicyMap<T, V>(j, policies);
  p = Minus<T, V>(j["id"].get<string>(), policies);
}

/*
{
    "type": "Divide",
    "id": "d1",
    "policies": {
      "i1": {
        "eager": true
      }
    }
}
*/

template <class T, class V>
void to_json(json& j, const Divide<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
  addPoliciesToJson<T, V>(j, p.getPolicies());
}

template <class T, class V>
void from_json(const json& j, Divide<T, V>& p) {
  map<string, typename Operator<T, V>::InputPolicy> policies;
  updateInputPolicyMap<T, V>(j, policies);
  p = Divide<T, V>(j["id"].get<string>(), policies);
}

/*
{
    "type": "Linear",
    "id": "l1",
    "coeff": [1,2,3,4],
    "policies": {
      "i1": {
        "eager": true
      }
    }
}
*/

template <class T, class V>
void to_json(json& j, const Linear<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"coeff", p.coeff}};
  addPoliciesToJson<T, V>(j, p.getPolicies());
}

template <class T, class V>
void from_json(const json& j, Linear<T, V>& p) {
  map<string, typename Operator<T, V>::InputPolicy> policies;
  updateInputPolicyMap<T, V>(j, policies);
  p = Linear<T, V>(j["id"].get<string>(), j["coeff"].get<vector<V>>(), policies);
}

/*
{
    "type": "AutoRegressive",
    "id": "ar",
    "coeff": [1,2,3,4]
}
*/

template <class T, class V>
void to_json(json& j, const AutoRegressive<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"coeff", p.coeff}};
}

template <class T, class V>
void from_json(const json& j, AutoRegressive<T, V>& p) {
  p = AutoRegressive<T, V>(j["id"].get<string>(), j["coeff"].get<vector<V>>());
}

/*
{
    "type": "GreaterThan",
    "id": "gt",
    "x": 0.5
}
*/

template <class T, class V>
void to_json(json& j, const GreaterThan<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"x", p.x0}};
}

template <class T, class V>
void from_json(const json& j, GreaterThan<T, V>& p) {
  p = GreaterThan<T, V>(j["id"].get<string>(), j["x"].get<V>());
}

/*
{
    "type": "LessThan",
    "id": "lt",
    "x": 0.5
}
*/

template <class T, class V>
void to_json(json& j, const LessThan<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"x", p.x0}};
}

template <class T, class V>
void from_json(const json& j, LessThan<T, V>& p) {
  p = LessThan<T, V>(j["id"].get<string>(), j["x"].get<V>());
}

/*
{
    "type": "Accumulator",
    "id": "ac"
}
*/

template <class T, class V>
void to_json(json& j, const Accumulator<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Accumulator<T, V>& p) {
  p = Accumulator<T, V>(j["id"].get<string>());
}

/*
{
    "type": "Count",
    "id": "cn"
}
*/

template <class T, class V>
void to_json(json& j, const Count<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Count<T, V>& p) {
  p = Count<T, V>(j["id"].get<string>());
}

/*
{
    "type": "Difference",
    "id": "diff"
}
*/

template <class T, class V>
void to_json(json& j, const Difference<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T, class V>
void from_json(const json& j, Difference<T, V>& p) {
  p = Difference<T, V>(j["id"].get<string>());
}

/*
{
    "type": "RelativeStrengthIndex",
    "id": "rsi"
    "n": 200
}
*/

template <class T, class V>
void to_json(json& j, const RelativeStrengthIndex<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.getMaxSize()}};
}

template <class T, class V>
void from_json(const json& j, RelativeStrengthIndex<T, V>& p) {
  p = RelativeStrengthIndex<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

/* Operators serialization - deserialization - end */

std::string FactoryOp::createPipeline(std::string const& id, std::string const& json_program) {
  try {
    pipelines.emplace(id, createPipeline(json_program));
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
  op_registry_add<Divide<std::uint64_t, double>, json>();
  op_registry_add<Linear<std::uint64_t, double>, json>();
  op_registry_add<AutoRegressive<std::uint64_t, double>, json>();
  op_registry_add<Output_opt<std::uint64_t, double>, json>();
  op_registry_add<GreaterThan<std::uint64_t, double>, json>();
  op_registry_add<LessThan<std::uint64_t, double>, json>();
  op_registry_add<Accumulator<std::uint64_t, double>, json>();
  op_registry_add<Count<std::uint64_t, double>, json>();
  op_registry_add<Difference<std::uint64_t, double>, json>();
  op_registry_add<RelativeStrengthIndex<std::uint64_t, double>, json>();

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
