
#include "rtbot/FactoryOp.h"
#include "rtbot/Operator.h"
#include "rtbot/tools/MovingAverage.h"
#include "rtbot/tools/PeakDetector.h"
#include "rtbot/tools/Input.h"
#include "rtbot/tools/CosineResampler.h"
#include "rtbot/tools/HermiteResampler.h"
#include "rtbot/Join.h"
#include "rtbot/Output.h"

#include <iostream>
#include <nlohmann/json.hpp>


using json = nlohmann::json;

namespace rtbot {

void to_json(json& j, const Input& p) {
        j = json{ {"type", p.typeName()}, {"id", p.id} };
}

void from_json(const json& j, Input& p) {
    j.at("id").get_to(p.id);
    p.n = Input::size;
}

void to_json(json& j, const CosineResampler& p) {
        j = json{ {"type", p.typeName()}, {"id", p.id}, {"dt", p.dt} };
}

void from_json(const json& j, CosineResampler& p) {
    j.at("id").get_to(p.id);
    j.at("dt").get_to(p.dt);
    p.n = CosineResampler::size;
    p.carryOver = CosineResampler::initinalCarryOver;
}

void to_json(json& j, const HermiteResampler& p) {
        j = json{ {"type", p.typeName()}, {"id", p.id}, {"dt", p.dt} };
}

void from_json(const json& j, HermiteResampler& p) {
    j.at("id").get_to(p.id);
    j.at("dt").get_to(p.dt);
    p.n = HermiteResampler::size;
    p.carryOver = HermiteResampler::initinalCarryOver;
    p.iteration = HermiteResampler::initialIteration;
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Output_opt,id);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MovingAverage,id,n);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PeakDetector,id,n);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Join<double>,id,nInput);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Difference,id);


std::string FactoryOp::createPipeline(std::string const& id, std::string const&  json_program)
{
    try
    {
        pipelines.emplace(id, createPipeline(json_program));
        return "";
    }
    catch (const nlohmann::json::parse_error& e)
    {
        // output exception information
        std::cout << "message: " << e.what() << '\n'
                  << "exception id: " << e.id << '\n'
                  << "byte position of error: " << e.byte << std::endl;
        return std::string("Unable to parse program: ") + e.what();
    }
}


/// register some the operators. Notice that this can be done on any constructor
/// whenever we create a static instance later, as  below:
FactoryOp::FactoryOp()
{
    op_registry_add< Input, nlohmann::json >();
    op_registry_add< CosineResampler, nlohmann::json >();
    op_registry_add< HermiteResampler, nlohmann::json >();
    op_registry_add< MovingAverage, nlohmann::json >();
    op_registry_add< PeakDetector , nlohmann::json >();
    op_registry_add< Join<>       , nlohmann::json >();
    op_registry_add< Difference   , nlohmann::json >();
    op_registry_add< Output_opt   , nlohmann::json >();

    nlohmann::json j;
    for(auto const& it : op_registry()) {
        nlohmann::json ji=nlohmann::json::parse(it.second.to_string_default());
        j.push_back(ji);
    }
    std::ofstream out("op_list.json");
    out<<std::setw(4)<<j<<"\n";
}

static FactoryOp factory;

Op_ptr<> FactoryOp::readOp(const std::string &program)
{
    auto json=nlohmann::json::parse(program);
    string type=json.at("type");
    auto it = op_registry().find(type);
    if (it == op_registry().end())
        throw std::runtime_error(string("invalid Operator type while parsing ") + type );
    return it->second.from_string(program);
}

std::string FactoryOp::writeOp(Op_ptr<> const& op)
{
    string type=op->typeName();
    nlohmann::json j;
    j["type"]=type;
    auto it = op_registry().find(type);
    if (it == op_registry().end())
        throw std::runtime_error(string("invalid Operator type while parsing ") + type );
    return it->second.to_string(op);
}


}


