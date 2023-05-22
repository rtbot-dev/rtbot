#include "rtbot/CostFunction.h"
#include <nlohmann/json.hpp>

namespace rtbot {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CostFunction::ParamData, op_id, paramName, current, lower, upper);

vector<CostFunction::ParamData> CostFunction::createParamData(std::string const& prog_json)
{
    auto prog=nlohmann::json::parse(prog_json);
    map<string, nlohmann::json&> op_list;
    for(nlohmann::json& x : prog.at("operators"))
        op_list.emplace(x.at("id"), x);

    vector<CostFunction::ParamData> out;
    for(ParamData x : prog.at("optimization")) {
        x.current=op_list.at(x.op_id).at(x.paramName);
        out.push_back(x);
    }
    return out;
}


string CostFunction::get_prog_json(vector<double> const& params) const
{
    auto prog2=nlohmann::json::parse(prog_json);
    map<string, nlohmann::json&> op_list;
    for(nlohmann::json& x : prog2.at("operators"))
        op_list.emplace(x.at("id"), x);
    for(auto i=0u; i<params.size(); i++) {
        const auto& par=paramsData.at(i);
        op_list.at(par.op_id).at(par.paramName)=params[i];
    }
    return prog2.dump();
}


}
