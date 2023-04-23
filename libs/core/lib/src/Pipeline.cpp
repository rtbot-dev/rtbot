#include "rtbot/Pipeline.h"
#include "rtbot/FactoryOp.h"

#include<nlohmann/json.hpp>

namespace rtbot {

struct OpConnection
{
    string from;
    string to;
    int toPort=-1;
    int fromPort=-1;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(OpConnection,from,to,toPort,fromPort);

Pipeline::Pipeline(const std::string &json_string)
{
    auto json = nlohmann::json::parse(json_string);
    for(const nlohmann::json& x : json.at("operators")) {
        auto it = all_op.emplace(x.at("id"), FactoryOp::readOp(x.dump().c_str()) );
        if (x.at("type")=="Input")
            input=it.first->second.get();
        else if (x.at("type")=="Output") {
            output=dynamic_cast<decltype(output)>(it.first->second.get());
            output->out=&out;
        }
    }

    // connections
    for(const OpConnection x : json.at("connections"))
        all_op.at(x.from)->connect(all_op.at(x.to).get(), x.toPort, x.fromPort);

}

}
