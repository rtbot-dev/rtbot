
#include "rtbot/FactoryOp.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "rtbot/Input.h"
#include "rtbot/Join.h"
#include "rtbot/Operator.h"
#include "rtbot/Output.h"
#include "rtbot/std/CosineResampler.h"
#include "rtbot/std/HermiteResampler.h"
#include "rtbot/std/MovingAverage.h"
#include "rtbot/std/PeakDetector.h"
#include "rtbot/std/StandardDeviation.h"

using json = nlohmann::json;

namespace rtbot {

template <class T = double>
void to_json(json& j, const Input<T>& p) { j = json{{"type", p.typeName()}, {"id", p.id}}; }

template <class T = double>
void from_json(const json& j, Input<T>& p) {
  j.at("id").get_to(p.id);
  p.n = Input<T>::size;
}

template <class T = double>
void to_json(json& j, const CosineResampler<T>& p) { j = json{{"type", p.typeName()}, {"id", p.id}, {"dt", p.dt}}; }

template <class T = double>
void from_json(const json& j, CosineResampler<T>& p) {
  j.at("id").get_to(p.id);
  j.at("dt").get_to(p.dt);
  p.n = CosineResampler<T>::size;
  p.carryOver = 0;
}

template <class T = double>
void to_json(json& j, const HermiteResampler<T>& p) { j = json{{"type", p.typeName()}, {"id", p.id}, {"dt", p.dt}}; }

template <class T = double>
void from_json(const json& j, HermiteResampler<T>& p) {
  j.at("id").get_to(p.id);
  j.at("dt").get_to(p.dt);
  p.n = HermiteResampler<T>::size;
  p.carryOver = 0;
  p.iteration = 0;
}

template <class T = double>
void to_json(json& j, const StandardDeviation<T>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.n}};
}

template <class T = double>
void from_json(const json& j, StandardDeviation<T>& p) {
  j.at("id").get_to(p.id);
  j.at("n").get_to(p.n);
}

template <class T = double>
void to_json(json& j, const MovingAverage<T>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.n}};
}

template <class T = double>
void from_json(const json& j, MovingAverage<T>& p) {
  j.at("id").get_to(p.id);
  j.at("n").get_to(p.n);  
}

template <class T = double>
void to_json(json& j, const Join<T>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}, {"n", p.nInput}};
}

template <class T = double>
void from_json(const json& j, Join<T>& p) {
  j.at("id").get_to(p.id);
  j.at("nInput").get_to(p.nInput);
}

template <class T = double>
void to_json(json& j, const Output_opt<T>& p) {
  j = json{{"type", p.typeName()}, {"id", p.id}};
}

template <class T = double>
void from_json(const json& j, Output_opt<T>& p) {
  j.at("id").get_to(p.id);
}


NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PeakDetector, id, n);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Difference, id);

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
  op_registry_add<Input<double>, nlohmann::json>();
  op_registry_add<CosineResampler<double>, nlohmann::json>();
  op_registry_add<HermiteResampler<double>, nlohmann::json>();
  op_registry_add<MovingAverage<double>, nlohmann::json>();
  op_registry_add<PeakDetector, nlohmann::json>();
  op_registry_add<Join<double>, nlohmann::json>();
  op_registry_add<Difference, nlohmann::json>();
  op_registry_add<Output_opt<double>, nlohmann::json>();

  nlohmann::json j;
  for (auto const& it : op_registry()) {
    nlohmann::json ji = nlohmann::json::parse(it.second.to_string_default());
    j.push_back(ji);
  }
  std::ofstream out("op_list.json");
  out << std::setw(4) << j << "\n";
}

static FactoryOp factory;

Op_ptr<> FactoryOp::readOp(const std::string& program) {
  auto json = nlohmann::json::parse(program);
  string type = json.at("type");
  auto it = op_registry().find(type);
  if (it == op_registry().end()) throw std::runtime_error(string("invalid Operator type while parsing ") + type);
  return it->second.from_string(program);
}

std::string FactoryOp::writeOp(Op_ptr<> const& op) {
  string type = op->typeName();
  nlohmann::json j;
  j["type"] = type;
  auto it = op_registry().find(type);
  if (it == op_registry().end()) throw std::runtime_error(string("invalid Operator type while parsing ") + type);
  return it->second.to_string(op);
}

}  // namespace rtbot
