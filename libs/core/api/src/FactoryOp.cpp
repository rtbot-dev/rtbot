
#include "rtbot/FactoryOp.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Operator.h"
#include "rtbot/Output.h"
#include "rtbot/std/AutoRegressive.h"
#include "rtbot/std/CosineResampler.h"
#include "rtbot/std/Difference.h"
#include "rtbot/std/HermiteResampler.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "rtbot/std/StandardDeviation.h"

using json = nlohmann::json;

namespace rtbot {

template <class T, class V>
void to_json(json& j, const Input<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

/*
{
    "type": "Input",
    "id": "in"
}
*/

template <class T, class V>
void from_json(const json& j, Input<T, V>& p) {
  p = Input<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const CosineResampler<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"dt", p.dt}};
}

/*
{
    "type": "CosineResampler",
    "id": "cr"
    "dt": 100
}
*/

template <class T, class V>
void from_json(const json& j, CosineResampler<T, V>& p) {
  p = CosineResampler<T, V>(j["id"].get<string>(), j["dt"].get<T>());
}

template <class T, class V>
void to_json(json& j, const HermiteResampler<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"dt", p.dt}};
}

/*
{
    "type": "HermiteResampler",
    "id": "hr"
    "dt": 100
}
*/

template <class T, class V>
void from_json(const json& j, HermiteResampler<T, V>& p) {
  p = HermiteResampler<T, V>(j["id"].get<string>(), j["dt"].get<T>());
}

template <class T, class V>
void to_json(json& j, const StandardDeviation<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.n}};
}

/*
{
    "type": "StandardDeviation",
    "id": "sd"
    "n": 5
}
*/

template <class T, class V>
void from_json(const json& j, StandardDeviation<T, V>& p) {
  p = StandardDeviation<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const MovingAverage<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.n}};
}

/*
{
    "type": "MovingAverage",
    "id": "ma"
    "n": 5
}
*/

template <class T, class V>
void from_json(const json& j, MovingAverage<T, V>& p) {
  p = MovingAverage<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const Join<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"numPorts", p.numPorts}};
}

/*
{
    "type": "Join",
    "id": "j1"
    "numPorts": 2
}
*/

template <class T, class V>
void from_json(const json& j, Join<T, V>& p) {
  p = Join<T, V>(j["id"].get<string>(), j["numPorts"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const Output_opt<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

/*
{
    "type": "Output",
    "id": "opt"
}
*/

template <class T, class V>
void from_json(const json& j, Output_opt<T, V>& p) {
  p = Output_opt<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const PeakDetector<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.n}};
}

/*
{
    "type": "PeakDetector",
    "id": "p1",
    "n": 5
}
*/

template <class T, class V>
void from_json(const json& j, PeakDetector<T, V>& p) {
  p = PeakDetector<T, V>(j["id"].get<string>(), j["n"].get<size_t>());
}

template <class T, class V>
void to_json(json& j, const Difference<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

/*
{
    "type": "Difference",
    "id": "d1"
}
*/

template <class T, class V>
void from_json(const json& j, Difference<T, V>& p) {
  p = Difference<T, V>(j["id"].get<string>());
}

template <class T, class V>
void to_json(json& j, const AutoRegressive<T, V>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"coeff", p.coeff}};
}

/*
{
    "type": "AutoRegressive",
    "id": "ar"
    "coef": [1,2,3,4]
}
*/

template <class T, class V>
void from_json(const json& j, AutoRegressive<T, V>& p) {
  p = AutoRegressive<T, V>(j["id"].get<string>(), j["coeff"].get<std::vector<V>>());
}

std::string FactoryOp::createPipeline(std::string const& id, std::string const& json_program) {
  try {
    pipelines.emplace(id, createPipeline(json_program));
    return "";
  } catch (const nlohmann::json::parse_error& e) {
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
  op_registry_add<Input<std::uint64_t, double>, nlohmann::json>();
  op_registry_add<CosineResampler<std::uint64_t, double>, nlohmann::json>();
  op_registry_add<HermiteResampler<std::uint64_t, double>, nlohmann::json>();
  op_registry_add<MovingAverage<std::uint64_t, double>, nlohmann::json>();
  op_registry_add<PeakDetector<std::uint64_t, double>, nlohmann::json>();
  op_registry_add<Join<std::uint64_t, double>, nlohmann::json>();
  op_registry_add<Difference<std::uint64_t, double>, nlohmann::json>();
  op_registry_add<AutoRegressive<std::uint64_t, double>, nlohmann::json>();
  op_registry_add<Output_opt<std::uint64_t, double>, nlohmann::json>();

  nlohmann::json j;
  for (auto const& it : op_registry()) {
    nlohmann::json ji = nlohmann::json::parse(it.second.to_string_default());
    j.push_back(ji);
  }
  std::ofstream out("op_list.json");
  out << std::setw(4) << j << "\n";
}

static FactoryOp factory;

Op_ptr<std::uint64_t, double> FactoryOp::readOp(const std::string& program) {
  auto json = nlohmann::json::parse(program);
  string type = json.at("type");
  auto it = op_registry().find(type);
  if (it == op_registry().end()) throw std::runtime_error(string("invalid Operator type while parsing ") + type);
  return it->second.from_string(program);
}

std::string FactoryOp::writeOp(Op_ptr<std::uint64_t, double> const& op) {
  string type = op->typeName();
  nlohmann::json j;
  j["type"] = type;
  auto it = op_registry().find(type);
  if (it == op_registry().end()) throw std::runtime_error(string("invalid Operator type while parsing ") + type);
  return it->second.to_string(op);
}

}  // namespace rtbot
